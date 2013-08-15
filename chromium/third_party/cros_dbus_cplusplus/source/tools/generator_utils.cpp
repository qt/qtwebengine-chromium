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

#include <iostream>
#include <cstdlib>

#include <dbus/dbus.h>		// for DBUS_TYPE_*

#include "generator_utils.h"

using namespace std;

void underscorize(string &str)
{
	for (unsigned int i = 0; i < str.length(); ++i)
	{
		if (!isalpha(str[i]) && !isdigit(str[i])) str[i] = '_';
	}
}

std::string legalize(const std::string &str)
{
	string legal = str;
	underscorize(legal);
	// TODO: Check for other C++ keywords, as needed.
	if (legal == "new")
	{
		legal = "_" + legal;
	}
	return legal;
}

string stub_name(string name)
{
	underscorize(name);

	return "_" + name + "_stub";
}

const char *atomic_type_to_string(char t)
{
	static struct { char type; const char *name; } atos[] =
	{
		{ DBUS_TYPE_BYTE, "uint8_t" },
		{ DBUS_TYPE_BOOLEAN, "bool" },
		{ DBUS_TYPE_INT16, "int16_t" },
		{ DBUS_TYPE_UINT16, "uint16_t" },
		{ DBUS_TYPE_INT32, "int32_t" },
		{ DBUS_TYPE_UINT32, "uint32_t" },
		{ DBUS_TYPE_INT64, "int64_t" },
		{ DBUS_TYPE_UINT64, "uint64_t" },
		{ DBUS_TYPE_DOUBLE, "double" },
		{ DBUS_TYPE_STRING, "std::string" },
		{ DBUS_TYPE_UNIX_FD, "::DBus::FileDescriptor" },
		{ DBUS_TYPE_OBJECT_PATH, "::DBus::Path" },
		{ DBUS_TYPE_SIGNATURE, "::DBus::Signature" },
		{ DBUS_TYPE_VARIANT, "::DBus::Variant" },
		{ '\0', "" }
	};
	int i;

	for (i = 0; atos[i].type; ++i)
	{
		if (atos[i].type == t) break;
	}
	return atos[i].name;
}

void _parse_signature(const string &signature, string &type, unsigned int &i)
{
	for (; i < signature.length(); ++i)
	{
		switch (signature[i])
		{
			case 'a':
			{
				switch (signature[++i])
				{
					case '{':
					{
						type += "std::map< ";

						const char *atom = atomic_type_to_string(signature[++i]);
						if (!atom)
						{
							cerr << "invalid signature" << endl;
							exit(-1);
						}
						type += atom;
						type += ", ";
						++i;
						break;
					}
					default:
					{
						type += "std::vector< ";
						break;
					}
				}
				_parse_signature(signature, type, i);
				type += " >";
				continue;
			}
			case '(':	
			{
				type += "::DBus::Struct< ";
				++i;
				_parse_signature(signature, type, i);
				type += " >";
				if (signature[i+1])
				{
					type += ", ";
				}
				continue;
			}
			case ')':
			case '}':	
			{
				return;
			}
			default:
			{
				const char *atom = atomic_type_to_string(signature[i]);
				if (!atom)
				{
					cerr << "invalid signature" << endl;
					exit(-1);
				}
				type += atom;

				if (signature[i+1] != ')' && signature[i+1] != '}' && i+1 < signature.length())
				{
					type += ", ";
				}
				break;
			}
		}
	}
}

string signature_to_type(const string &signature)
{
	string type;
	unsigned int i = 0;
	_parse_signature(signature, type, i);
	return type;
}

bool is_primitive_type(const string &signature) {
	unsigned int len = signature.length();

	if (len == 0) {
		cerr << "invalid signature" << endl;
		exit(-1);
	}

	if (len > 1) {
		return false;
	}

	switch (signature[0]) {
		case DBUS_TYPE_BYTE:  // uint8_t
		case DBUS_TYPE_BOOLEAN:  // bool
		case DBUS_TYPE_INT16:  // int16_t
		case DBUS_TYPE_UINT16:  // uint16_t
		case DBUS_TYPE_INT32:  // int32_t
		case DBUS_TYPE_UINT32:  // uint32_t
		case DBUS_TYPE_INT64:  // int64_t
		case DBUS_TYPE_UINT64:  // uint64_t
		case DBUS_TYPE_DOUBLE:  // double
		case DBUS_TYPE_UNIX_FD: // int
			return true;
		default:
			return false;
	}
}
