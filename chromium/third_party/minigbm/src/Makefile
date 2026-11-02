# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

# TODO: Re-enable these warnings.
CFLAGS += -Wno-implicit-fallthrough -Wno-unreachable-code

PC_DEPS = libdrm
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -D_GNU_SOURCE=1
CFLAGS += -std=c99 -Wall -Wsign-compare -Wpointer-arith -Wcast-qual \
	  -Wcast-align -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64

# Dependencies that all gtest based unittests should have.
UNITTEST_LIBS := -lcap -lgtest -lgmock
UNITTEST_DEPS := gbm_unittest.o testrunner.o gbm.o dri.o drv_array_helpers.o drv_helpers.o drv.o backend_mock.o virtgpu_cross_domain.o virtgpu_virgl.o virtgpu.o msm.o vc4.o amdgpu.o i915.o mediatek.o dumb_driver.o

ifdef DRV_AMDGPU
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_amdgpu)
	LDLIBS += -ldrm_amdgpu -ldl
endif
ifeq ($(filter 1,$(DRV_I915) $(DRV_XE)),)
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_intel)
endif
ifdef DRV_MESON
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_meson)
endif
ifdef DRV_MSM
	CFLAGS += -ldl
endif
ifdef DRV_RADEON
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_radeon)
endif
ifdef DRV_ROCKCHIP
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_rockchip)
endif
ifdef DRV_VC4
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libdrm_vc4)
endif

CPPFLAGS += $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

DESTDIR ?= $(OUT)/install
LIBDIR ?= /usr/lib
INCLUDEDIR ?= /usr/include

MINIGBM_VERSION_MAJOR := 1
MINIGBM_VERSION := $(MINIGBM_VERSION_MAJOR).0.0
MINIGBM_FILENAME := libminigbm.so.$(MINIGBM_VERSION)

CC_LIBRARY($(MINIGBM_FILENAME)): LDFLAGS += -Wl,-soname,libgbm.so.$(MINIGBM_VERSION_MAJOR)
CC_LIBRARY($(MINIGBM_FILENAME)): $(C_OBJECTS) pkgconfig
CC_STATIC_LIBRARY(libminigbm.pie.a): $(C_OBJECTS)

all: CC_LIBRARY($(MINIGBM_FILENAME))

clean: CLEAN($(MINIGBM_FILENAME))

CXX_BINARY(gbm_unittest): CXXFLAGS += -Wno-write-strings \
						$(GTEST_CXXFLAGS)
CXX_BINARY(gbm_unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(gbm_unittest): $(UNITTEST_DEPS)
clean: CLEAN(gbm_unittest)
tests: TEST(CXX_BINARY(gbm_unittest))

define pkgconfig_contents
prefix=$(DESTDIR)
exec_prefix=$${prefix}
includedir=$${prefix}/$(INCLUDEDIR)
libdir=$${prefix}/$(LIBDIR)

Name: libgbm
Description: A small gbm implementation
Version: 18.0.0
Cflags: -I$${includedir}
Libs: -L$${libdir} -lgbm
Requires.private: libdrm >= 2.4.50
endef

.PHONY: pkgconfig
pkgconfig:
	@echo "generating $(OUT)gbm.pc"
	$(file > $(OUT)/gbm.pc,$(pkgconfig_contents))
clean:
	rm -f $(OUT)gbm.pc

install: all
	mkdir -p $(DESTDIR)/$(LIBDIR)
	install -D -m 755 $(OUT)/$(MINIGBM_FILENAME) $(DESTDIR)/$(LIBDIR)
	ln -sf $(MINIGBM_FILENAME) $(DESTDIR)/$(LIBDIR)/libgbm.so
	ln -sf $(MINIGBM_FILENAME) $(DESTDIR)/$(LIBDIR)/libgbm.so.$(MINIGBM_VERSION_MAJOR)
	ln -sf $(MINIGBM_FILENAME) $(DESTDIR)/$(LIBDIR)/libgbm.so.$(MINIGBM_VERSION)
	install -D -m 0644 $(OUT)/gbm.pc $(DESTDIR)/$(LIBDIR)/pkgconfig/gbm.pc
	install -D -m 0644 $(SRC)/gbm.h $(DESTDIR)/$(INCLUDEDIR)/gbm.h
	install -D -m 0644 $(SRC)/minigbm_helpers.h $(DESTDIR)/$(INCLUDEDIR)/minigbm/minigbm_helpers.h
