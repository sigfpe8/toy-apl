// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#ifndef _TOKEN_H_
#define _TOKEN_H_

// Represents one APL token
typedef struct {
	int	code;	// Main Unicode character
	int	flags;	// Various bit flags
	int	prefix;	// Char after prefix key 
} TOKEN;

// Maps a Unicode character (0-65535) to a token (0-127)
typedef struct {
	int	code;		// Unicode code point
	int	token;		// Corresponding token #
	int	next;		// Next entry in collision chain
} CHARMAP;

// Represents one alternate character for a token
typedef struct {
	int code;	// Alternate Unicode character
	int token;	// Corresponding token
} ALTCHAR;

// Represents one system variable/function name
typedef struct {
        char *pName;
        int  token;
        int  index;
} NameIndex;

#define	CHARHASH_SIZE	256
#define	CHARHASH_MASK	255
#define	CHARHASH(c)		((((c) >> 8) ^ ((c) & 0xFF)) & CHARHASH_MASK)

// Token flags
#define	ATOM	1
#define	MONADIC	2
#define	DYADIC	4
#define	BIADIC	(MONADIC | DYADIC)
#define	NAME	8
#define	OPER	16
#define	LDEL	32

// Special characters
#define	CHAR_HIGHMINUS	0x00AF
#define	CHAR_DELTA		0x2206

#define	IsMonadic(tk)	(AplTokens[tk].flags & MONADIC)
#define	IsDyadic(tk)	(AplTokens[tk].flags & DYADIC)
#define	IsAtom(tk)		(AplTokens[tk].flags & ATOM)
#define	IsEnd(tk)		((AplTokens[tk].flags & LDEL) || ((tk) == APL_END))
#define	IsToken(tk)		(AplTokens[tk].flags)
#define	IsAssign(tk)	((tk) == APL_LEFT_ARROW)
#define	IsNumber(c)		(isdigit(c) || (c) == CHAR_HIGHMINUS || (c) == '.')

// APL tokens
#define	APL_END				0
#define	APL_NUM				1
#define	APL_CHR				2
#define	APL_ARR				3
#define	APL_STR				4
#define	APL_VARNAM			5
#define	APL_VARINX			6
#define	APL_VARSYS			7
#define	APL_SYSFUN1			8
#define	APL_SYSFUN2			9

#define APL_NL				12

#define	APL_EPSILON			15
#define	APL_IOTA			16
#define	APL_RHO				17
#define	APL_UP_STILE		18
#define	APL_DOWN_STILE		19
#define	APL_LEFT_ARROW		20
#define	APL_UP_ARROW		21
#define	APL_RIGHT_ARROW		22
#define	APL_DOWN_ARROW		23
#define	APL_CIRCLE			24
#define	APL_UP_TACK			25
#define	APL_DOWN_TACK		26
#define	APL_GRADE_UP		27
#define	APL_GRADE_DOWN		28
#define	APL_HYDRANT			29
#define	APL_THORN			30
#define	APL_SLASH_BAR		31
#define	APL_BACKSLASH_BAR	32
#define	APL_EXCL_MARK		33
#define	APL_CIRCLE_STILE 	34
#define	APL_CIRCLE_BAR		35
#define	APL_TRANSPOSE		36
#define	APL_QUAD			37
#define	APL_QUOTE_QUAD		38
#define	APL_DOMINO			39
#define	APL_LEFT_PAREN		40
#define	APL_RIGHT_PAREN		41
#define	APL_STAR			42
#define	APL_PLUS			43
#define	APL_COMMA			44
#define	APL_MINUS			45
#define	APL_DOT				46
#define	APL_SLASH			47
#define	APL_LT_OR_EQUAL		48
#define	APL_NOT_EQUAL		49
#define	APL_GT_OR_EQUAL		50
#define	APL_AND				51
#define	APL_OR				52
#define	APL_NAND			53
#define	APL_NOR				54
#define	APL_TIMES			55
#define	APL_DIV				56
#define	APL_CIRCLE_STAR		57
#define	APL_COLON			58
#define	APL_SEMICOLON		59
#define	APL_LESS_THAN		60
#define	APL_EQUAL			61
#define	APL_GREATER_THAN	62
#define	APL_QUESTION_MARK	63
#define	APL_AT				64
#define	APL_LEFT_BRACKET	65
#define	APL_BACKSLASH		66
#define	APL_RIGHT_BRACKET	67
#define	APL_STILE			68
#define	APL_TILDE			69
#define	APL_DEL				70
#define	APL_LAMP			71
#define	APL_DIAMOND			72
#define	APL_JOT				73
#define	APL_ALPHA			74
#define	APL_OMEGA			75
#define	APL_DELTA			76
#define	APL_COMMA_BAR		77

extern TOKEN AplTokens[];

extern void token_init(void);
extern int	token_from_char(int code);

#endif // _TOKEN_H_

