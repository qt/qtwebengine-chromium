#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ef-echo-server.h"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>

static const char *ECHO_SERVER_NAME = "org.freedesktop.DBus.Examples.Echo";
static const char *ECHO_SERVER_PATH = "/org/freedesktop/DBus/Examples/Echo";

DBus::BusDispatcher dispatcher;


EchoServer::EchoServer(DBus::Connection &connection)
: DBus::ObjectAdaptor(connection, ECHO_SERVER_PATH, REGISTER_NOW, AVOID_EXCEPTIONS)
{
}

DBus::Message EchoServer::Random(const DBus::CallMessage &call)
{
	// Make a new tag that can be used to track this call
	DBus::Tag *tag = new DBus::Tag();

	// Set up a timeout to cause something to happen later
	// N.B. This example does not show how to delete the timeout
	DBus::DefaultTimeout *timeout =
		new DBus::DefaultTimeout(1000, false, &dispatcher);
	timeout->data(tag);
	DBus::Callback_Base<void, DBus::DefaultTimeout &> *cb =
		new DBus::Callback<EchoServer, void, DBus::DefaultTimeout &>(
			this, &EchoServer::RandomCallback);
	timeout->expired = cb;

	return DBus::TagMessage(tag);
}

void EchoServer::RandomCallback(DBus::DefaultTimeout &timeout)
{
	const DBus::Tag *tag = reinterpret_cast<DBus::Tag *>(timeout.data());

	Continuation *ret = find_continuation(tag);
	assert(ret);

	RandomWriteReply(ret->writer(), rand());
	return_now(ret);
	timeout.data(NULL);
	delete tag;
	timeout.enabled(false);
	// N.B. We cannot delete the timeout here, that would deadlock
}

DBus::Message EchoServer::Hello(const DBus::CallMessage &call, const std::string &name)
{
	return HelloMakeReply(call, "Hello " + name + "!");
}

DBus::Message EchoServer::Echo(const DBus::CallMessage &call, const DBus::Variant &value)
{
	this->Echoed(value);

	return EchoMakeReply(call, value);
}

struct CatData {
	CatData(FILE *h) : handle(h) {}
	FILE *handle;
	DBus::Tag tag;
};

DBus::Message EchoServer::Cat(const DBus::CallMessage &call, const std::string &file)
{
	FILE *handle = fopen(file.c_str(), "rb");

	if (!handle) return DBus::ErrorMessage(call,
					       "org.freedesktop.DBus.EchoDemo.ErrorFileNotFound",
					       "file not found");

	CatData *cat_data = new CatData(handle);
	// Set up a timeout to cause something to happen later
	// N.B. This example does not show how to delete the timeout
	DBus::DefaultTimeout *timeout =
		new DBus::DefaultTimeout(3000, false, &dispatcher);
	timeout->data(cat_data);
	DBus::Callback_Base<void, DBus::DefaultTimeout &> *cb =
		new DBus::Callback<EchoServer, void, DBus::DefaultTimeout &>(
			this, &EchoServer::CatCallback);
	timeout->expired = cb;

	return DBus::TagMessage(&cat_data->tag);
}

void EchoServer::CatCallback(DBus::DefaultTimeout &timeout)
{
	CatData *cat_data = reinterpret_cast<CatData *>(timeout.data());
	Continuation *ret = find_continuation(&cat_data->tag);
	assert(ret);

	uint8_t buff[1024];
	size_t nread = fread(buff, 1, sizeof(buff), cat_data->handle);
	fclose(cat_data->handle);

	if (nread >= 80) {
		// Arbitrary size chosen to show how to return an
		// error from a continuation.
		return_error(ret, DBus::Error("org.freedesktop.DBus.EchoDemo.ErrorFileTooBig",
					      "file too big"));
	} else {
		CatWriteReply(ret->writer(), std::vector< uint8_t > (buff, buff + nread));
		return_now(ret);
	}
	delete cat_data;
	timeout.enabled(false);
	// N.B. We cannot delete the timeout here, that would deadlock
}

DBus::Message EchoServer::Sum(const DBus::CallMessage &call, const std::vector<int32_t>& ints)
{
	int32_t sum = 0;

	for (size_t i = 0; i < ints.size(); ++i) sum += ints[i];

	return SumMakeReply(call, sum);
}

DBus::Message EchoServer::Info(const DBus::CallMessage &call)
{
	std::map< std::string, std::string > info;
	char hostname[HOST_NAME_MAX];

	gethostname(hostname, sizeof(hostname));
	info["hostname"] = hostname;
	info["username"] = getlogin();

	return InfoMakeReply(call, info);
}


void niam(int sig)
{
	dispatcher.leave();
}

int main()
{
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SessionBus();
	conn.request_name(ECHO_SERVER_NAME);

	EchoServer server(conn);

	dispatcher.enter();

	return 0;
}
