AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/mediatek \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_mediatek_la_LTLIBRARIES = libdrm_mediatek.la
libdrm_mediatek_ladir = $(libdir)
libdrm_mediatek_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_mediatek_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_mediatek_la_SOURCES = \
	mediatek_drm.c

libdrm_mediatekincludedir = ${includedir}/libdrm
libdrm_mediatekinclude_HEADERS = mediatek_drmif.h mediatek_drm.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_mediatek.pc
