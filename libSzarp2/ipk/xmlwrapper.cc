/* 
  SZARP: SCADA software 

*/
/*
 * IPK
 *
 * Krzysztof Goliñski <krzgol@newterm.pl>
 *
 * IPK 'TDefined' class implementation.
 *
 * $Id$
 */

#include <sstream>
 
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <string>
#include <dirent.h>
#include <assert.h>

#include "szarp_config.h"
#include "conversion.h"
#include "parcook_cfg.h"
#include "line_cfg.h"
#include "ptt_act.h"
#include "sender_cfg.h"
#include "raporter.h"
#include "ekrncor.h"
#include "liblog.h"

using namespace std;

void XMLWrapper::SetIgnoredTags(const char *i_list[]) {

	while (*i_list) {
		ignoredTags.insert(*i_list);
		++i_list;
	}
}

void XMLWrapper::SetIgnoredTrees(const char *i_list[]) {

	while (*i_list) {
		ignoredTrees.insert(*i_list);
		++i_list;
	}
}

bool XMLWrapper::AreValidAttr(const char* attr_list[]) {

	while (*attr_list) {
		neededAttr.insert(*attr_list);
		++attr_list;
	}

	return true;
}
bool XMLWrapper::NextTag() {
	int ans = 0;
	for (;;) {
		ans = xmlTextReaderRead(r);
		if (ans != 1)
			return false;
		name = xmlTextReaderConstName(r);
		if (ignoredTags.find(std::string((const char*) name)) == ignoredTags.end()) {
			if (ignoredTrees.find(std::string((const char*) name)) == ignoredTrees.end()) {
				return true;
			} else {
				xmlTextReaderNext(r);
			}
		}
	}
	return true;
}
bool XMLWrapper::IsTag(const char* n) {
	return xmlStrEqual( name , (const unsigned char*) n );
}

bool XMLWrapper::IsAttr(const char* a) {
	return xmlStrEqual( attr_name , (const unsigned char*) a );
}

bool XMLWrapper::IsFirstAttr() {
	int at = xmlTextReaderMoveToFirstAttribute(r);
	if (at > 0) {
		attr_name = xmlTextReaderConstName(r);
		attr = xmlTextReaderConstValue(r);
		return true;
	}
	attr_name = NULL;
	attr = NULL;

	return false;
}

bool XMLWrapper::IsNextAttr() {
	int at = xmlTextReaderMoveToNextAttribute(r);
	if (at > 0) {
		attr_name = xmlTextReaderConstName(r);
		attr = xmlTextReaderConstValue(r);
		return true;
	}
	attr_name = NULL;
	attr = NULL;

	return false;
}

bool XMLWrapper::IsBeginTag() {
	return xmlTextReaderNodeType(r) == XML_READER_TYPE_ELEMENT;
}

bool XMLWrapper::IsEndTag() {
	return xmlTextReaderNodeType(r) == XML_READER_TYPE_END_ELEMENT;
}

bool XMLWrapper::IsEmptyTag() {
	return xmlTextReaderIsEmptyElement(r);
}

const xmlChar* XMLWrapper::GetAttr() {
	return attr;
}
const xmlChar* XMLWrapper::GetAttrName() {
	return attr_name;
}

const xmlChar* XMLWrapper::GetTagName() {
	return name;
}

void XMLWrapper::XMLError(const char *text, int prior) {
	sz_log(prior,"XML file error: %s (line,%d)", text, xmlTextReaderGetParserLineNumber(r));
}
