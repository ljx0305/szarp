#include "cfgdealer_handler.h"
#include "zmq.hpp"
#include "liblog.h"
#include <boost/property_tree/xml_parser.hpp>


void ConfigDealerHandler::parseArgs(const ArgsManager& args_mgr) {
	ipc_info = IPCInfo{
		*args_mgr.get<std::string>("parcook_path"),
		*args_mgr.get<std::string>("linex_cfg")
	};

	device_no = *args_mgr.get<unsigned int>("device-no");
	IPKPath = *args_mgr.get<std::string>("IPK");
	single = args_mgr.has("single");
}

ConfigDealerHandler::ConfigDealerHandler(const ArgsManager& args_mgr) {
	parseArgs(args_mgr);

	zmq::context_t context(1);
	zmq::socket_t socket(context, ZMQ_REQ);

	int timeout_ms = args_mgr.get<int>("cfgdealer_connection_timeout").get_value_or(10000);
	socket.setsockopt(ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

	// int delay = 1;
	// socket.setsockopt(ZMQ_DELAY_ATTACH_ON_CONNECT, &delay, sizeof(delay));

	auto cfgdealer_address = args_mgr.get<std::string>("cfgdealer-address").get_value_or("tcp://localhost:5555");
	socket.connect(cfgdealer_address.c_str());
	
	auto d_str = std::to_string(device_no);
	zmq::message_t request (d_str.size());
	memcpy(request.data(), d_str.c_str(), d_str.size());

	socket.send(request);

	//  Get the reply.
	zmq::message_t reply;
	socket.recv (&reply);

	std::string conf_str{std::move((char*) reply.data()), reply.size()};
	if (conf_str.empty()) {
		sz_log(0, "Got no configuration from cfgdealer (is it running?).");
		throw std::runtime_error("Got no configuration from cfgdealer (is it running?).");
	}

	std::istringstream ss(std::move(conf_str));
	boost::property_tree::ptree conf_tree;
	boost::property_tree::read_xml(ss, conf_tree);

	for (const auto& params_child: conf_tree.get_child("params")) {
		if (params_child.first == "sends") {
			for (const auto& sends_child: params_child.second) {
				if (sends_child.first == "param") {
					parseSentParam(sends_child.second);
				}
			}
		}
	}

	for (const auto& params_child: conf_tree.get_child("params")) {
		if (params_child.first == "device") {
			parseDevice(params_child.second);
		}
	}
}

void ConfigDealerHandler::parseDevice(const boost::property_tree::ptree& device_conf) {
	device_xml = SC::S2A(SC::L2S(device_conf.data()));
	ipc_offset = device_conf.get<int>("<xmlattr>.initial_param_no") - 1;
	device = new CDevice();
	device->parseXML(device_conf);
	// TODO: configure timeval
	for (const auto& unit_conf: device_conf) {
		if (unit_conf.first == "unit") {
			units.push_back(new CUnit(this, unit_conf.second, ipc_offset + params_count));
			params_count += units.back()->GetParamsCount();
		}
	}
}

void ConfigDealerHandler::parseSentParam(const boost::property_tree::ptree& send) {
	auto param = new CParam(send, send.get<size_t>("<xmlattr>.ipc_ind"));
	sent_params[param->GetName()] = param;
	sends_inds.push_back(param->GetIpcInd());
}


ConfigDealerHandler::CUnit::CUnit(ConfigDealerHandler* parent, const boost::property_tree::ptree& conf, size_t offset) {
	TAttribHolder::parseXML(conf);
	unit_no = getAttribute<size_t>("unit_no");

	params.reserve(conf.count("param"));
	sends.reserve(conf.count("send"));

	size_t param_no = offset;
	for (const auto& param_conf: conf) {
		if (param_conf.first == "param") {
			params.push_back(new CParam(param_conf.second, param_no++));
		} else if (param_conf.first == "send") {
			sends.push_back(new CSend(parent, param_conf.second));
		}
	}
}


ConfigDealerHandler::CParam::CParam(const boost::property_tree::ptree& conf, size_t _ipc_ind): ipc_ind(_ipc_ind) {
	TAttribHolder::parseXML(conf);
	SetName(SC::L2S(conf.get<std::string>("<xmlattr>.name")));
}


ConfigDealerHandler::CSend::CSend(ConfigDealerHandler* _parent, const boost::property_tree::ptree& conf): parent(_parent) {
	TAttribHolder::parseXML(conf);
	sent_param_name = SC::L2S(getAttribute<std::string>("param", ""));
	ParseSentParam();
}

void ConfigDealerHandler::CSend::ParseSentParam() {
	const std::vector<std::string> ignored_params = {"extra:address"};
	IPCParamInfo* param_to_send = GetParamToSend();
	if (!param_to_send) return;

	std::vector<std::pair<std::string, std::string>> attributes = param_to_send->getAllAttributes();
	for (const auto& attr_pair: attributes) {
		const std::string& attr_name = attr_pair.first;
		if (std::find(ignored_params.begin(), ignored_params.end(), attr_name) == ignored_params.end()) continue;

		if (!hasAttribute(attr_name)) {
			const std::string& attr_val = attr_pair.second;
			storeAttribute(attr_name, attr_val);
		}
	}
}


