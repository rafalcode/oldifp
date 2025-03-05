#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(ifp-line, 0.3)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([ifp.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL

# Checks for libraries.
AM_ICONV
AM_PATH_LIBUSB(0.1.7, , AC_MSG_ERROR([libusb is required.]))

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([langinfo.h limits.h locale.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([memset mkdir nl_langinfo setlocale strdup strncasecmp strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
