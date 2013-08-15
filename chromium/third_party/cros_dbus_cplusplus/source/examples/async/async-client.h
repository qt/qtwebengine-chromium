#ifndef __DEMO_ASYNC_CLIENT_H
#define __DEMO_ASYNC_CLIENT_H

#include <dbus-c++/dbus.h>
#include "async-client-glue.h"

class AsyncClient
: public org::freedesktop::DBus::AsyncDemo_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

	AsyncClient(DBus::Connection &connection, const char *path, const char *name);

	void Echoed(const DBus::Variant &value);

        virtual void HelloCallback(const std::string& greeting, const ::DBus::Error& e, void* data);
        virtual void SumCallback(const int32_t& sum, const ::DBus::Error& e, void* data);
        virtual void SplitStringCallback(const std::string& out_string1, const std::string& out_string2, const ::DBus::Error& e, void* data);
	virtual void Timed_WaitCallback(const ::DBus::Error& e, void* data);

	virtual void Change_Sig(const std::string& newval);
};

#endif//__DEMO_ASYNC_CLIENT_H
