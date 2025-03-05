# Configure paths for libusb
# Yamashiro, Jun <yamajun at ofug.net>     2005-11-27
# stolen from Manish Singh and Frank Belew
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_LIBUSB([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libusb, and define LIBUSB_CFLAGS and LIBUSB_LIBS
dnl
AC_DEFUN([AM_PATH_LIBUSB],
[
AC_ARG_WITH(libusb-prefix,[ --with-libusb-prefix=DIR	Prefix where libusb installed (optional)],
	    libusb_prefix="$withval", libusb_prefix="")
AC_ARG_WITH(libusb-exec-prefix,[ --with-libusb-exec-prefix=DIR	Exec where libusb installed (optional)],
	    libusb_exec_prefix="$withval", libusb_exec_prefix="")

    if test x$libusb_exec_prefix != x ; then
	libusb_config_args="$libusb_config_args --exec-prefix=$libusb_exec_prefix"
	if test x${LIBUSB_CONFIG+set} != xset ; then
	    LIBUSB_CONFIG=$libusb_exec_prefix/bin/libusb-config
	fi
    fi
    if test x$libusb_prefix != x ; then
	libusb_config_args="$libusb_config_args --prefix=$libusb_prefix"
	if test x${LIBUSB_CONFIG+set} != xset ; then
	    LIBUSB_CONFIG=$libusb_prefix/bin/libusb-config
	fi
    fi

    AC_PATH_PROG(LIBUSB_CONFIG, libusb-config, no)
    min_libusb_version=ifelse([$1], ,0.1.0,$1)
    AC_MSG_CHECKING(for libusb - version >= $min_libusb_version)
    no_libusb=""
    if test "$LIBUSB_CONFIG" = "no" ; then
	no_libusb="yes"
    else
	LIBUSB_CFLAGS=`$LIBUSB_CONFIG $libusb_config_args --cflags`
	LIBUSB_LIBS=`$LIBUSB_CONFIG $libusb_config_args --libs`

	libusb_major_version=`$LIBUSB_CONFIG $libusb_args --version | \
		sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
	libusb_minor_version=`$LIBUSB_CONFIG $libusb_args --version | \
		sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
	libusb_micro_version=`$LIBUSB_CONFIG $libusb_args --version | \
		sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
	if test "x$enable_libusbtext" = "xyes" ; then
	    ac_save_CFLAGS="$CFLAGS"
	    ac_save_LIBS="$LIBS"
	    CFLAGS="$CFLAGS $LIBUSB_CFLAGS"
	    LIBS="$LIBS $LIBUSB_LIBS"
dnl
dnl Now check if the installed LIBUSB is sufficiently new. (Also sanity
dnl checks the results of imlib-config to some extent
dnl
      rm -f conf.libusbtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.libusbtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_libusb_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libusb_version");
     exit(1);
   }

    if (($libusb_major_version > major) ||
        (($libusb_major_version == major) && ($libusb_minor_version > minor)) ||
	(($libusb_major_version == major) && ($libusb_minor_version == minor) &&
	($libusb_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'libusb-config --version' returned %d.%d, but the minimum version\n", $libusb_major_version, $libusb_minor_version);
      printf("*** of libusb required is %d.%d. If libusb-config is correct, then it is\n", major, minor);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If libusb-config was wrong, set the environment variable LIBUSB_CONFIG\n");
      printf("*** to point to the correct copy of libusb-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_libusb=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libusb" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$LIBUSB_CONFIG" = "no" ; then
       echo "*** The libusb-config script installed by libusb could not be found"
       echo "*** If libusb was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the LIBUSB_CONFIG environment variable to the"
       echo "*** full path to libusb-config."
     else
       if test -f conf.libusbtest ; then
        :
       else
          echo "*** Could not run libusb test program, checking why..."
          CFLAGS="$CFLAGS $LIBUSB_CFLAGS"
          LIBS="$LIBS $LIBUSB_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <usb.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding libusb or finding the wrong"
          echo "*** version of libusb. If it is not finding libusb, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means libusb was incorrectly installed"
          echo "*** or that you have moved libusb since it was installed. In the latter case, you"
          echo "*** may want to edit the libusb-config script: $LIBUSB_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBUSB_CFLAGS=""
     LIBUSB_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(LIBUSB_CFLAGS)
  AC_SUBST(LIBUSB_LIBS)
  rm -f conf.libusbtest
])

