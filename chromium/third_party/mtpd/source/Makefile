# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP = dbusxx-xml2cpp

# Create a generator for DBUS-C++ headers.
# TODO(wad): integrate into common.mk.
GEN_DBUSXX(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
		mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
		$(DBUSXX_XML2CPP) $< --adaptor=$(TARGET_OR_MEMBER))

PROTOC ?= protoc
BASE_VER ?= 180609
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 libmtp \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER) protobuf
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/include -I$(SRC) -I$(OUT) -I$(OUT)mtpd_server $(PC_CFLAGS)
LDLIBS += -ludev $(PC_LIBS)

## protobuffer targets
# Detailed instructions on how to work with these rules can be found in the
# common-mk package.  These rules are for including protobufs that live in the
# dbus directory of the system_api repo. To add a new one from this location to
# the build, just add the name of the generated .cc file to the bindings
# variable and include approriate depends targets[.
#
# To add protobufs from a different location then you need to copy all the proto
# variables and use a new prefix for them, setting the path variable as you
# need.
#
# TODO(thestig) Use protobuf macros once crosbug.com/30056 has been fixed.
SYSTEM_API_PROTO_BINDINGS = mtp_file_entry.pb.cc \
                            mtp_storage_info.pb.cc
SYSTEM_API_PROTO_PATH = $(SYSROOT)/usr/include/chromeos/dbus
SYSTEM_API_PROTO_HEADERS = $(patsubst %.cc,%.h,$(SYSTEM_API_PROTO_BINDINGS))
SYSTEM_API_PROTO_OBJS = $(patsubst %.cc,%.o,$(SYSTEM_API_PROTO_BINDINGS))
$(SYSTEM_API_PROTO_HEADERS): %.h: %.cc ;
$(SYSTEM_API_PROTO_BINDINGS): %.pb.cc: $(SYSTEM_API_PROTO_PATH)/%.proto
	$(PROTOC) --proto_path=$(SYSTEM_API_PROTO_PATH) --cpp_out=. $<
clean: CLEAN($(SYSTEM_API_PROTO_BINDINGS))
clean: CLEAN($(SYSTEM_API_PROTO_HEADERS))
clean: CLEAN($(SYSTEM_API_PROTO_OBJS))
# Add rules for compiling generated protobuffer code, as the CXX_OBJECTS list
# is built before these source files exists and, as such, does not contain them.
$(eval $(call add_object_rules,$(SYSTEM_API_PROTO_OBJS),CXX,cc))

GEN_DBUSXX(mtpd_server/mtpd_server.h): $(SRC)/mtpd.xml
mtpd_server/mtpd_server.h: GEN_DBUSXX(mtpd_server/mtpd_server.h)
clean: CLEAN(mtpd_server/mtpd_server.h)

# Require the header to be generated first.
$(patsubst %.o,%.o.depends,$(CXX_OBJECTS)): mtpd_server/mtpd_server.h \
	mtp_file_entry.pb.h mtp_storage_info.pb.h

CXX_BINARY(mtpd): $(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS)) \
	mtp_file_entry.pb.o mtp_storage_info.pb.o
clean: CLEAN(mtpd)

CXX_BINARY(mtpd_testrunner): $(filter-out %main.o,$(CXX_OBJECTS)) \
	mtp_file_entry.pb.o mtp_storage_info.pb.o
CXX_BINARY(mtpd_testrunner): LDLIBS += -lgtest -lgmock -lpthread
clean: CLEAN(mtpd_testrunner)

# Some shortcuts
mtpd: CXX_BINARY(mtpd)
all: mtpd

user_tests: TEST(CXX_BINARY(mtpd_testrunner))
tests: user_tests
