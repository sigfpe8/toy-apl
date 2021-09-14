// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include "utf8.h"

// Output buffer 'buf' must have room for 4 bytes
// Return # of bytes output to buffer or
//        0 if not enough space
int wchar_to_utf8(int wchr, char *buf, int len)
{
	if (wchr <= 0x7f) {		// 7 bits
		if (len < 1)
			return 0;
		*buf = wchr;
		return 1;
	}

	if (wchr <= 0x07FF) {	// 11 bits
		if (len < 2)
			return 0;
		*buf = UTF8_2BYTES | ((wchr >> 6) & 0x1F);	// 5 bits
		*(buf+1) = UTF8_CONT | (wchr & 0x3F);		// 6 bits
		return 2;
	}
	
	if (wchr <= 0xFFFF) {	// 16 bits
		if (len < 3)
			return 0;
		*buf = UTF8_3BYTES | ((wchr >> 12) & 0x0F);	 // 4 bits
		*(buf + 1) = UTF8_CONT | ((wchr >> 6) & 0x3F);// 6 bits
		*(buf + 2) = UTF8_CONT | (wchr & 0x3F);		 // 6 bits
		return 3;
	}

	if (len < 4)			// 21 bits
		return 0;
	*buf = UTF8_4BYTES | ((wchr >> 18) & 0x07);		// 3 bits
	*(buf + 1) = UTF8_CONT | ((wchr >> 12) & 0x3F);	// 6 bits
	*(buf + 2) = UTF8_CONT | ((wchr >>  6) & 0x3F);	// 6 bits
	*(buf + 3) = UTF8_CONT | (wchr & 0x3F);			// 6 bits
	return 4;
}

int utf8_to_wchar(char *pchr, int avl, int *plen)
{
	int code;
	int byte;
	int len;

	byte = *pchr;	// 1st byte
	if ((byte & UTF8_MASK2) == UTF8_2BYTES) {
		if (avl < 2 ||
		   (pchr[1] & ~UTF8_MASK) != UTF8_CONT)
			return 0;
		code = ((byte & ~UTF8_MASK2) << 6) |
			   (pchr[1] & UTF8_MASK);
		len = 2;
	} else if ((byte & UTF8_MASK3) == UTF8_3BYTES) {
		if (avl < 3 ||
		   (pchr[1] & ~UTF8_MASK) != UTF8_CONT ||
		   (pchr[2] & ~UTF8_MASK) != UTF8_CONT)
			return 0;
		code = ((byte & ~UTF8_MASK3) << 12) |
			   ((pchr[1] & UTF8_MASK) << 6) |
			    (pchr[2] & UTF8_MASK);
		len = 3;
	} else if ((byte & UTF8_MASK4) == UTF8_4BYTES) {
		if (avl < 4 ||
		   (pchr[1] & ~UTF8_MASK) != UTF8_CONT ||
		   (pchr[2] & ~UTF8_MASK) != UTF8_CONT ||
		   (pchr[3] & ~UTF8_MASK) != UTF8_CONT)
			return 0;
		code = ((byte & ~UTF8_MASK4) << 18) |
			   ((pchr[1] & UTF8_MASK) << 12) |
			   ((pchr[2] & UTF8_MASK) << 6) |
			    (pchr[3] & UTF8_MASK);
		len = 4;
	} else
		return 0;

	*plen = len;

	return code;
}
