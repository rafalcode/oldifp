## $Id: Makefile.am,v 1.3 2005/12/10 07:35:04 yamajun Exp $

EXTRA_DIST = $(man1_MANS) $(pkgdata_DATA)

bin_PROGRAMS = ifp
ifp_SOURCES = ifp.c ifp_routines.c ifp_routines.h
ifp_CFLAGS = @LIBUSB_CFLAGS@
ifp_LDADD = ./libunicodehack.a @LIBICONV@ @LIBUSB_LIBS@

noinst_LIBRARIES = libunicodehack.a
libunicodehack_a_SOURCES = unicodehack.c unicodehack.h
libunicodehack_a_CFLAGS = @CPPFLAGS@

man1_MANS = ifp.1
pkgdata_DATA = NEWS README TIPS nonroot.sh

