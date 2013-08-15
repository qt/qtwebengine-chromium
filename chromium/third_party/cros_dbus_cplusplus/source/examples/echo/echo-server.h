#ifndef __DEMO_ECHO_SERVER_H
#define __DEMO_ECHO_SERVER_H

#include <dbus-c++/dbus.h>
#include "echo-server-glue.h"

class EchoServer
: public org::freedesktop::DBus::EchoDemo_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

	EchoServer(DBus::Connection &connection);

	int32_t Random(DBus::Error &error);

	std::string Hello(const std::string &name, DBus::Error &error);

	DBus::Variant Echo(const DBus::Variant &value, DBus::Error &error);

	std::vector< uint8_t > Cat(const std::string &file, DBus::Error &error);

	int32_t Sum(const std::vector<int32_t> & ints, DBus::Error &error);

	std::map< std::string, std::string > Info(DBus::Error &error);
};

#endif//__DEMO_ECHO_SERVER_H
