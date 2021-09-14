// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <stdlib.h>
#include <string.h>

#include "token.h"

TOKEN AplTokens[] = {
/* 000 */	{ 0,		0,			0	},	// APL_END - End of expression
/* 001 */	{ 0,		ATOM,		0	},	// APL_NUM - Number
/* 002 */	{ 0,		ATOM,		0	},	// APL_CHR - Character
/* 003 */	{ 0,		ATOM,		0	},	// APL_ARR - Array
/* 004 */	{ 0,		ATOM,		0	},	// APL_STR - String
/* 005 */	{ 0,		ATOM,		0	},	// APL_VARNAM - Variable by name
/* 006 */	{ 0,		ATOM,		0	},	// APL_VARIND - Variable by index
/* 007 */	{ 0,		ATOM,		0	},	// APL_SYSVAR - System variable
/* 008 */	{ 0,		MONADIC,	0	},	// APL_SYSFUN1 - Monadic system function
/* 009 */	{ 0,		DYADIC,		0	},	// APL_SYSFUN2 - Dyadic system function
/* 010 */	{ 0,		0,			0	},	// Available
/* 011 */	{ 0,		0,			0	},	// Available
/* 012 */	{ 0,		LDEL,		0	},	// APL_NL - New line
/* 013 */	{ 0,		0,			0	},	// Available
/* 014 */	{ 0,		0,			0	},	// Available
/* 015 */	{ 0x220a,	DYADIC,		'e'	},	// ∊
/* 016 */	{ 0x2373,	BIADIC,		'i'	},	// ⍳
/* 017 */	{ 0x2374,	BIADIC,		'r'	},	// ⍴
/* 018 */	{ 0x2308,	BIADIC,		's'	},	// ⌈
/* 019 */	{ 0x230a,	BIADIC,		'd'	},	// ⌊
/* 020 */	{ 0x2190,	DYADIC,		'['	},	// ←
/* 021 */	{ 0x2191,	DYADIC,		'y'	},	// ↑
/* 022 */	{ 0x2192,	LDEL,		']'	},	// →
/* 023 */	{ 0x2193,	DYADIC,		'u'	},	// ↓
/* 024 */	{ 0x25CB,	BIADIC,		'o'	},	// ○
/* 025 */	{ 0x22a5,	DYADIC,		'b'},	// ⊥
/* 026 */	{ 0x22a4,	DYADIC,		'n'},	// ⊤
/* 027 */	{ 0x234b,	MONADIC,	'$'	},	// ⍋
/* 028 */	{ 0x2352,	MONADIC,	'#'	},	// ⍒
/* 029 */	{ 0x234e,	MONADIC,	';'	},	// ⍎
/* 030 */	{ 0x2355,	BIADIC,		'\''},	// ⍕
/* 031 */	{ 0x233F,	DYADIC,		'/'	},	// ⌿
/* 032 */	{ 0x2340,	DYADIC,		'.'	},	// ⍀
/* 033 */	{ 0x0021,	BIADIC,		0	},	// !
/* 034 */	{ 0x233D,	BIADIC,		'%'	},	// ⌽
/* 035 */	{ 0x2296,	BIADIC,		'&'	},	// ⊖
/* 036 */	{ 0x2349,	BIADIC,		'^'	},	// ⍉
/* 037 */	{ 0x2395,	ATOM,		'l'	},	// ⎕
/* 038 */	{ 0x235E,	ATOM,		'{'	},	// ⍞
/* 039 */	{ 0x2339,	BIADIC,		'+'	},	// ⌹
/* 040 */	{ 0x0028,	LDEL,		0	},	// (
/* 041 */	{ 0x0029,	ATOM,		0	},	// )
/* 042 */	{ 0x002A,	BIADIC,		0	},	// *
/* 043 */	{ 0x002B,	BIADIC,		0	},	// +
/* 044 */	{ 0x002C,	BIADIC,		0	},	// ,
/* 045 */	{ 0x002D,	BIADIC,		0	},	// -
/* 046 */	{ 0x002E,	OPER,		0	},	// .
/* 047 */	{ 0x002F,	DYADIC,		0	},	// /
/* 048 */	{ 0x2264,	DYADIC,		'4'	},	// ≤
/* 049 */	{ 0x2260,	DYADIC,		'8'	},	// ≠
/* 050 */	{ 0x2265,	DYADIC,		'6'	},	// ≥
/* 051 */	{ 0x2227,	DYADIC,		'0'	},	// ∧
/* 052 */	{ 0x2228,	DYADIC,		'9'	},	// ∨
/* 053 */	{ 0x2372,	DYADIC,		')'	},	// ⍲
/* 054 */	{ 0x2371,	DYADIC,		'('	},	// ⍱
/* 055 */	{ 0x00D7,	BIADIC,		'-'	},	// ×
/* 056 */	{ 0x00F7,	BIADIC,		'='	},	// ÷
/* 057 */	{ 0x235F,	BIADIC,		'*'	},	// ⍟
/* 058 */	{ 0x003A,	0,			0	},	// :
/* 059 */	{ 0x003B,	LDEL,		0	},	// ;
/* 060 */	{ 0x003C,	DYADIC,		0	},	// <
/* 061 */	{ 0x003D,	DYADIC,		0	},	// =
/* 062 */	{ 0x003E,	DYADIC,		0	},	// >
/* 063 */	{ 0x003F,	BIADIC,		0	},	// ?
/* 064 */	{ 0x0040,	0,			0	},	// @
/* 065 */	{ 0x005B,	LDEL,		0	},	// [
/* 066 */	{ 0x005C,	DYADIC,		0	},	//
/* 067 */	{ 0x005D,	ATOM,		0	},	// ]
/* 068 */	{ 0x007C,	BIADIC,		0	},	// |
/* 069 */	{ 0x007E,	MONADIC,	0	},	// ~
/* 070 */	{ 0x2207,	0,			'g'	},	// ∇
/* 071 */	{ 0x235D,	0,			','	},	// ⍝
/* 072 */	{ 0x22C4,	LDEL,		'`'	},	// ⋄
/* 073 */	{ 0x2218,	OPER,		'j'	},	// ∘
/* 074 */	{ 0x2379,	0,			'a'	},	// ⍺
/* 075 */	{ 0x2375,	0,			'w'	},	// ⍵
/* 076 */	{ 0x2206,	0,			'h'	},	// ∆
/* 077 */	{ 0x236a,	DYADIC,		'<'	},	// ⍪
};

int	num_tokens = sizeof(AplTokens) / sizeof(AplTokens[0]);

ALTCHAR AplAltChars[] = {
	{ 0x22c6,	APL_STAR		},	// *
	{ 0x2223,	APL_STILE		},	// |
	{ 0x23a2,	APL_STILE		},	// |
	{ 0x223c,	APL_TILDE		},	// ~
	{ 0x005e,	APL_AND			},	// ^
};

int	num_altchars = sizeof(AplAltChars) / sizeof(AplAltChars[0]);

#ifdef	TOKEN_DEBUG
#include <stdio.h>
#include "utf8.h"
static void token_list_charmap(void);
static void token_list_tokens(void);
#endif

int *charhash_table;		// Pointer to the charmap hash table (int [])
CHARMAP *charmap_table;		// Pointer to the charmap itself (CHARMAP [])
int num_charmaps;			// # of entries in charmap
int num_tokchars;			// # of tokens associated with chars

void token_init(void)
{
	// In the hash table and in the 'next' field we only store the index
	// of the CHARMAP entry in charmap_table[].
	charhash_table = malloc(sizeof(int) * CHARHASH_SIZE);
	memset(charhash_table, 0, sizeof(int) * CHARHASH_SIZE);

	// The first entry in charmap_table[] (index 0) is not used, so that
	// 0 means end of chain. The number of entries in this table is thus
	// 1 + tokens-associated-with-chars + alternate-chars.

	// Find the number of tokens that are associated with a character (some,
	// like APL_VARNAM, are internal and don't map to a character).
	for (int i = 0; i < num_tokens; ++i)
		if (AplTokens[i].code)
			++num_tokchars;

	// The number of alternate characters is defined above

	// Allocate the character map
	num_charmaps = 1 + num_tokchars + num_altchars;
	charmap_table = malloc(sizeof(CHARMAP) * num_charmaps);

	// Insert all tokens that have an associated character
	int index = 0;
	for (int tok = 0; tok < num_tokens; ++tok) {
		int code = AplTokens[tok].code;
		if (code) {
			int hash = CHARHASH(code);
			CHARMAP *p = &charmap_table[++index];
			p->code = code;
			p->token = tok;
			p->next = charhash_table[hash];
			charhash_table[hash] = index;
		}
	}

	// Insert all alternate characters
	// E.g. ^ (U+005e) is an alternate character for APL_AND
	for (int i = 0; i < num_altchars; ++i) {
		int code = AplAltChars[i].code;
		int hash = CHARHASH(code);
		CHARMAP *p = &charmap_table[++index];
		p->code = code;
		p->token = AplAltChars[i].token;
		p->next = charhash_table[hash];
		charhash_table[hash] = index;
	}

#ifdef	TOKEN_DEBUG
	token_list_charmap();
	token_list_tokens();
#endif
}

// Given a Unicode character,
//   return: its corresponding token or
//           0 if no token is associated with that char
int token_from_char(int code)
{
	CHARMAP *p;
	int hash = CHARHASH(code);
	int index = charhash_table[hash];

	while (index) {
		p = &charmap_table[index];
		if (p->code == code)
			return p->token;
		index = p->next;
	}

	return 0;
}

#ifdef	TOKEN_DEBUG
static void token_list_charmap(void)
{
	int maxchain = 0;

	for (int hash = 0; hash < CHARHASH_SIZE; ++hash) {
		int index = charhash_table[hash];
		printf("\n%02x -> %3d", hash, index);
		int n = 0;
		while (index) {
			CHARMAP *p = &charmap_table[index];
			printf(" (%04x, %d)", p->code, p->token);
			index = p->next;
			++n;
		}
		if (maxchain < n)
			maxchain = n;
	}

	printf("\n\nMaxchain = %d\n", maxchain);
}

static void token_list_tokens(void)
{
	char buffer[16];
	int ntoks = 0;

	for (int code = 0; code < 65536; ++code) {
		int token = token_from_char(code);
		if (token) {
			int len = wchar_to_utf8(code, buffer, sizeof(buffer)-1);
			buffer[len] = 0;
			printf("\n %04x %s -> %d", code, buffer, token);
			++ntoks;
		}
	}
	printf("\n%d tokens\n", ntoks);
}

#endif	// TOKEN_DEBUG
