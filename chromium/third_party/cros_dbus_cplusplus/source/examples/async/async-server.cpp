#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "async-server.h"
#include "dbus-c++/error.h"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>

static const char *ASYNC_SERVER_NAME = "org.freedesktop.DBus.Examples.Async";
static const char *ASYNC_SERVER_PATH = "/org/freedesktop/DBus/Examples/Async";

AsyncServer::AsyncServer(DBus::Connection &connection)
: DBus::ObjectAdaptor(connection, ASYNC_SERVER_PATH)
{
}

std::string AsyncServer::Hello(const std::string &name, DBus::Error &error)
{
	return "Hello " + name + "!";
}

int32_t AsyncServer::Sum(const std::vector<int32_t>& ints, DBus::Error &error)
{
	int32_t sum = 0;

	for (size_t i = 0; i < ints.size(); ++i) sum += ints[i];

	return sum;
}

void AsyncServer::SplitString(const std::string& input, std::string &string1, std::string &string2, DBus::Error &error)
{
	if (input.size() == 0) {
		error.set("org.freedesktop.DBus.Error.InvalidArgs", "input string must not be empty");
		return;
	}
	size_t cpos = input.find(',');
	if (cpos == std::string::npos) {
		string1 = input;
		string2 = "";
	} else {
		string1 = input.substr(0, cpos);
		string2 = input.substr(cpos+1);
	}
}

void AsyncServer::Timed_Wait(const int32_t &seconds, DBus::Error &error)
{
	if (seconds)
		sleep(seconds);
}

DBus::BusDispatcher dispatcher;

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
	conn.request_name(ASYNC_SERVER_NAME);

	AsyncServer server(conn);

	dispatcher.enter();

	return 0;
}
