#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "async-client.h"
#include <cstdio>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <signal.h>

using namespace std;

static const char *ASYNC_SERVER_NAME = "org.freedesktop.DBus.Examples.Async";
static const char *ASYNC_SERVER_PATH = "/org/freedesktop/DBus/Examples/Async";
static AsyncClient *client;

AsyncClient::AsyncClient(DBus::Connection &connection,
			 const char *path, const char *name)
: DBus::ObjectProxy(connection, path, name)
{
}

void AsyncClient::HelloCallback(const std::string& greeting,
				const ::DBus::Error& e, void*)
{
	cout << "reply from Hello: " << greeting << endl;
        cout << "Hello path=" << object()->path() << endl;
        cout << "Hello service=" << object()->service() << endl;
}

void AsyncClient::SumCallback(const int32_t& sum, const ::DBus::Error& e, void*)
{
	cout << "reply from Sum: " << sum << endl;
}

void AsyncClient::SplitStringCallback(const std::string& string1,
				      const std::string& string2,
				      const ::DBus::Error& e, void* data)
{
	if (e.is_set())
		cout << "error from SplitString: " << e.name()
		     << ": " << e.message() << endl;
	else
		cout << "reply from SplitString: \"" << string1 << "\" \""
		     << string2 << "\"" << endl;
	cout << "  data = " << data << endl;
}

DBus::BusDispatcher dispatcher;

void AsyncClient::Timed_WaitCallback(const ::DBus::Error& e, void *data)
{
	cout << "reply from Timed_Wait" << endl;
	dispatcher.leave();
}

void AsyncClient::Change_Sig(const std::string& newval)
{
	cout << "Change_Sig signal with newval=" << newval << endl;
}

void *do_method_calls(void *arg)
{
	AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);

        client->Hello("Obi-Wan", NULL);
	cout << "Called Hello method" << endl;

        int32_t nums[] = {3, 1, -5, 7, 23};

        std::vector<int32_t> numvec(&nums[0], &nums[5]);

        client->Sum(numvec, NULL);
	cout << "Called Sum method" << endl;

	string out1, out2;
	// TODO(ers) using a timeout results in a deadlock if the timeout
	// expires
	client->SplitString("first part,second part", (void *)98/*, 1000*/);
	cout << "Called SplitString method" << endl;

	client->SplitString("", NULL);
	cout << "Called SplitString method with empty string" << endl;

	client->Timed_Wait(4, NULL);
	cout << "Called Timed_Wait" << endl;

	cout << "done " << endl;

	return NULL;
}

void niam(int sig)
{
	dispatcher.leave();
}

int main()
{
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::_init_threading();

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SessionBus();

	client = new AsyncClient(conn, ASYNC_SERVER_PATH, ASYNC_SERVER_NAME);

	pthread_t thread;
	pthread_create(&thread, NULL, do_method_calls, client);

	dispatcher.enter();

	cout << "terminating" << endl;

	delete client;

	return 0;
}
