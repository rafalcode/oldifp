/* $Id: unicodehack.h,v 1.5 2005/12/10 08:34:28 yamajun Exp $ */

#ifndef IFPLINE_UNICODEHACK_H
#define IFPLINE_UNICODEHACK_H

extern int locale2unicode(char*, size_t, const char*, size_t);
extern int unicode2locale(char*, size_t, const char*, size_t);

#endif	// IFPLINE_UNICODEHACK_H
