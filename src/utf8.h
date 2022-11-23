// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#ifndef	_UTF8_H
#define	_UTF8_H

#define UTF8_MASK2		0xE0	// 111x xxxx
#define	UTF8_2BYTES		0xC0	// 110x xxxx

#define UTF8_MASK3		0xF0	// 1111 xxxx
#define	UTF8_3BYTES		0xE0	// 1110 xxxx

#define UTF8_MASK4		0xF8	// 1111 1xxx
#define	UTF8_4BYTES		0xF0	// 1111 0xxx

#define UTF8_MASK		0x3F	// 0011 1111
#define	UTF8_CONT		0x80	// 10xx xxxx

extern int wchar_to_utf8(int wchr, char *buf, int len);
extern int utf8_to_wchar(const char *pchr, int avl, int *plen);
extern int utf8_len(const char *buf, const char *end);

#endif	// _UTF8_H
