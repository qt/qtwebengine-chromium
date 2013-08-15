/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <ctemplate/template.h>

#include <dbus/dbus.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xml2cpp.h"
#include "generate_stubs.h"

using namespace ctemplate;
using namespace std;
using namespace DBus;

//typedef map<string,string> TypeCache;

void usage(char *argv0)
{
	char *prog = strrchr(argv0, '/');
	if (prog == NULL)
		prog = argv0;
	else
		++prog;
	cerr << endl << "Usage: " << endl;
	cerr << "  " << prog << " <xmlfile> --proxy=<outfile.h> [ --proxy-template=<template.tpl> ] [ --templatedir=<template-dir> ] [ --[no]sync ] [ --[no]async ]" << endl << endl;
	cerr << "  --OR--" << endl << endl;
	cerr << "  " << prog << " <xmlfile> --adaptor=<outfile.h> [ --adaptor-template=<template.tpl> ] [ --templatedir=<template-dir> ]" << endl;
	cerr << endl << "Flags which can be repeated:" << endl;
	cerr << "    --define macroname value" << endl;
	exit(-1);
}

/*int char_to_atomic_type(char t)
{
	if (strchr("ybnqiuxtdsgavre", t))
		return t;

	return DBUS_TYPE_INVALID;
}*/



/*bool is_atomic_type(const string &type)
{
	return type.length() == 1 && char_to_atomic_type(type[0]) != DBUS_TYPE_INVALID;
}*/


int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		usage(argv[0]);
	}

	bool proxy_mode, adaptor_mode, async_proxy_mode, sync_proxy_mode;
	char *proxy, *adaptor;
	const char *proxy_template = "proxy-stubs.tpl";
	const char *adaptor_template = "adaptor-stubs.tpl";
	std::vector< pair<string, string> > macros;

	sync_proxy_mode = true;
	async_proxy_mode = false;
	proxy_mode = false;
	proxy = 0;

	adaptor_mode = false;
	adaptor = 0;

	for (int a = 1; a < argc; ++a)
	{
		if (!strncmp(argv[a], "--proxy=", 8))
		{
			proxy_mode = true;
			proxy = argv[a] + 8;
		}
		else if (!strncmp(argv[a], "--adaptor=", 10))
		{
			adaptor_mode = true;
			adaptor = argv[a] + 10;
		}
		else if (!strncmp(argv[a], "--proxy-template=", 17))
		{
			proxy_template = argv[a] + 17;
		}
		else if (!strncmp(argv[a], "--adaptor-template=", 19))
		{
			adaptor_template = argv[a] + 19;
		}
		else if (!strncmp(argv[a], "--templatedir=", 14))
		{
			Template::AddAlternateTemplateRootDirectory(argv[a] + 14);
		}
		else if (!strcmp(argv[a], "--async"))
		{
			async_proxy_mode = true;
		}
		else if (!strcmp(argv[a], "--sync"))
		{
			sync_proxy_mode = true;
		}
		else if (!strcmp(argv[a], "--noasync"))
		{
			async_proxy_mode = false;
		}
		else if (!strcmp(argv[a], "--define") && (a + 2) < argc)
		{
			const char *name = argv[++a];
			const char *value = argv[++a];
			macros.push_back(pair<string, string>(name, value));
		}
		else if (!strcmp(argv[a], "--nosync"))
		{
			sync_proxy_mode = false;
		}
	}
	Template::AddAlternateTemplateRootDirectory(DATADIR);

	if ((!proxy_mode && !adaptor_mode) || (proxy_mode && adaptor_mode))
		usage(argv[0]);

	ifstream xmlfile(argv[1]);

	if (xmlfile.bad())
	{
		cerr << "unable to open file " << argv[1] << endl;
		return -1;
	}

	Xml::Document doc;

	try
	{
		xmlfile >> doc;
		//cout << doc.to_xml();
	}
	catch(Xml::Error &e)
	{
		cerr << "error parsing " << argv[1] << ": " << e.what() << endl;
		return -1;
	}

	if (!doc.root)
	{
		cerr << "empty document" << endl;
		return -1;
	}

	if (proxy_mode)
		generate_stubs(doc, proxy, macros, sync_proxy_mode, async_proxy_mode, proxy_template);
	else if (adaptor_mode)
		generate_stubs(doc, adaptor, macros, true, true, adaptor_template);

	return 0;
}
