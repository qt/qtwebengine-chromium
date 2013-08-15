#ifndef __DEMO_ECHO_SERVER_H
#define __DEMO_ECHO_SERVER_H

#include <dbus-c++/dbus.h>
#include "ef-echo-server-glue.h"

class EchoServer
: public org::freedesktop::DBus::EchoDemo_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

	EchoServer(DBus::Connection &connection);

	DBus::Message Random(const DBus::CallMessage &call);
	void RandomCallback(DBus::DefaultTimeout &timeout);

	DBus::Message Hello(const DBus::CallMessage &call, const std::string &name);

	DBus::Message Echo(const DBus::CallMessage &call, const DBus::Variant &value);

	DBus::Message Cat(const DBus::CallMessage &call, const std::string &file);
	void CatCallback(DBus::DefaultTimeout &timeout);

	DBus::Message Sum(const DBus::CallMessage &call, const std::vector<int32_t> & ints);

	DBus::Message Info(const DBus::CallMessage &call);
};

#endif//__DEMO_ECHO_SERVER_H
