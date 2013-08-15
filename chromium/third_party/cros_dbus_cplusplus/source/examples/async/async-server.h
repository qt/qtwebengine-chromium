#ifndef __DEMO_ASYNC_SERVER_H
#define __DEMO_ASYNC_SERVER_H

#include <dbus-c++/dbus.h>
#include "async-server-glue.h"

class AsyncServer
: public org::freedesktop::DBus::AsyncDemo_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

	AsyncServer(DBus::Connection &connection);

	virtual std::string Hello(const std::string &name, DBus::Error &error);

	virtual int32_t Sum(const std::vector<int32_t> & ints, DBus::Error &error);

        virtual void SplitString(const std::string& input, std::string &string1, std::string &string2, DBus::Error &error);

	virtual void Timed_Wait(const int32_t &seconds, DBus::Error &error);
};

#endif//__DEMO_ASYNC_SERVER_H
