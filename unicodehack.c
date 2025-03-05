/* $Id: unicodehack.c,v 1.6 2005/12/03 15:23:31 yamajun Exp $ */

static char const rcsid[] = "$Id: unicodehack.c,v 1.6 2005/12/03 15:23:31 yamajun Exp $";

#include <stdio.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>

#include "config.h"
#include "unicodehack.h"

#ifdef HAVE_ICONV
#include <iconv.h>

/**
 * @return less then zero: error.
 */
int locale2unicode(char *dst, size_t dstln, const char *src, size_t srcln)
{
    iconv_t cd;
    int retval;

    setlocale(LC_ALL, "");
    if ( ( cd = iconv_open("UTF-16LE", nl_langinfo(CODESET)) ) == (iconv_t)-1) {
	perror("iconv_open");
	return -1;
    }

    // so this next branch does nothing as you can see. But the check has to be got right.
    // note we do lose the constness of the string.
    // if ( (retval = iconv(cd, (const char **)&src, &srcln, &dst, &dstln)) == -1) {
    if ( (retval = iconv(cd, (char **__restrict)&src, &srcln, &dst, &dstln)) == -1) {
	// XXX ignore error message "iconv: Illiegal byte sequence"
	// with GNU libiconv.  No effect for output in now.
	//perror("iconv");
    }

    if (iconv_close(cd) == -1) {
	perror("iconv_close");
	return -1;
    }

    return retval;
}

/**
 * @return less then zero: error.
 */
int unicode2locale(char *dst, size_t dstln, const char *src, size_t srcln)
{
    iconv_t cd;
    int retval;

    setlocale(LC_ALL, "");
    if ( ( cd = iconv_open(nl_langinfo(CODESET), "UTF-16LE") ) == (iconv_t)-1) {
	perror("iconv_open");
	return -1;
    }

    if ( (retval = iconv(cd, (char **__restrict)&src, &srcln, &dst, &dstln)) == -1) {
	// XXX ignore error message "iconv: Illiegal byte sequence"
	// with GNU libiconv.  No effect for output in now.
	//perror("iconv");
    }

    if (iconv_close(cd) == -1) {
	perror("iconv_close");
	return -1;
    }

    return retval;
}

#else	// no iconv(3)
int locale2unicode(char *dest, size_t dstln, const char *src, size_t srcln)
{
    int i = 0, j = 0;
    while (src[j] != 0) {
        dest[i++] = src[j++];
        dest[i++] = 0;
    }
    dest[i++] = 0;
    dest[i++] = 0;
    return i;
}

int unicode2locale(char *dest, size_t dstln, const char *src, size_t srcln)
{
    int i = 0, j = 0;
    while ((src[j] != 0) || (src[j+1] != 0)) {
        dest[i++] = src[j];
        j += 2;
    }
    dest[i++] = 0;
    return i;
}

#endif	// HAVE_ICONV
