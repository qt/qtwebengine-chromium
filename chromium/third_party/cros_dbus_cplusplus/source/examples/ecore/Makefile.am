EXTRA_DIST = 

if CROSS_COMPILING
dbusxx_xml2cpp = dbusxx-xml2cpp
else
dbusxx_xml2cpp = $(top_builddir)/tools/dbusxx-xml2cpp
DBUSXX_FLAGS = --templatedir=$(top_builddir)/tools
endif

AM_CPPFLAGS = -I$(top_srcdir)/include $(ecore_CFLAGS) $(xml_CFLAGS) -I$(top_srcdir)/tools

if ENABLE_ECORE
noinst_PROGRAMS = dbus_ecore
endif

dbus_ecore_SOURCES = dbus_ecore-glue.h dbus_ecore.h dbus_ecore.cpp $(top_srcdir)/tools/xml.cpp
dbus_ecore_LDADD = $(top_builddir)/src/libdbus-c++-1.la $(ecore_LIBS) $(xml_LIBS)

dbus_ecore-glue.h: $(top_srcdir)/data/org.freedesktop.DBus.xml
	$(dbusxx_xml2cpp) $^ --proxy=$@ $(DBUSXX_FLAGS)

BUILT_SOURCES = dbus_ecore-glue.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	Makefile.in

