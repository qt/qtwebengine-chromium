EXTRA_DIST = 

if CROSS_COMPILING
dbusxx_xml2cpp = dbusxx-xml2cpp
else
dbusxx_xml2cpp = $(top_builddir)/tools/dbusxx-xml2cpp
DBUSXX_FLAGS = --templatedir=$(top_builddir)/tools
endif

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include $(gtkmm_CFLAGS) -I$(top_srcdir)/tools

if HAVE_GTKMM
noinst_PROGRAMS = dbus-browser
endif

dbus_browser_SOURCES = dbus-glue.h dbus-browser.h dbus-browser.cpp $(top_srcdir)/tools/xml.cpp
dbus_browser_LDADD = $(top_builddir)/src/libdbus-c++-1.la $(gtkmm_LIBS)

dbus-glue.h: $(top_srcdir)/data/org.freedesktop.DBus.xml
	$(dbusxx_xml2cpp) $^ --proxy=$@ $(DBUSXX_FLAGS)

BUILT_SOURCES = dbus-glue.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	Makefile.in
