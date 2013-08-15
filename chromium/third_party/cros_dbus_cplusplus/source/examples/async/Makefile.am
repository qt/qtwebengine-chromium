EXTRA_DIST = README async-introspect.xml

if CROSS_COMPILING
dbusxx_xml2cpp = dbusxx-xml2cpp
else
dbusxx_xml2cpp = $(top_builddir)/tools/dbusxx-xml2cpp
DBUSXX_FLAGS = --templatedir=$(top_builddir)/tools
endif

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include $(glib_CFLAGS)

noinst_PROGRAMS = async-server

async_server_SOURCES = async-server-glue.h async-server.h async-server.cpp
async_server_LDADD = $(top_builddir)/src/libdbus-c++-1.la

async-server-glue.h: async-introspect.xml
	$(dbusxx_xml2cpp) $^ --adaptor=$@ $(DBUSXX_FLAGS)

noinst_PROGRAMS += async-client

async_client_SOURCES = async-client-glue.h async-client.h async-client.cpp
async_client_LDADD = $(top_builddir)/src/libdbus-c++-1.la @PTHREAD_LIBS@ $(glib_LIBS)
async_client_CXXFLAGS = @PTHREAD_CFLAGS@

async-client-glue.h: async-introspect.xml
	$(dbusxx_xml2cpp) $^ --async --nosync --proxy=$@ $(DBUSXX_FLAGS)

BUILT_SOURCES = async-server-glue.h async-client-glue.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	Makefile.in
