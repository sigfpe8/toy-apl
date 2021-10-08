// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "apl.h"
#include "error.h"
#include "token.h"
#include "utf8.h"

char *apchLexMsg[] =
{
	"No error",					/*  0 */
	"Invalid token",			/*  1 */
	"Too many literals",		/*  2 */
	"Invalid number",			/*  3 */
	"Invalid string",			/*  4 */
	"Code full",				/*  5 */
	"Invalid name",				/*  6 */
	"Invalid function header",	/*  7 */
	"Invalid del command",		/*  8 */
	"Invalid label",			/*  9 */
	"Function not defined",		/* 10 */
	"Function already defined",	/* 11 */
	"Name conflict",			/* 12 */
	"String too long",			/* 13 */
	"Invalid system name"		/* 14 */
};

NameIndex SysNames[] = {
	{ "a",			APL_VARSYS,		SYS_A		},
	{ "ct",			APL_VARSYS,		SYS_CT		},
	{ "d",			APL_VARSYS,		SYS_D		},
	{ "dbg",		APL_VARSYS,		SYS_DBG		},
	{ "ident",		APL_SYSFUN1,	SYS_IDENT	},
	{ "io",			APL_VARSYS,		SYS_IO		},
	{ "lu",			APL_SYSFUN1,	SYS_LU		},
	{ "pid",		APL_VARSYS,		SYS_PID		},
	{ "pp",			APL_VARSYS,		SYS_PP		},
	{ "rref",		APL_SYSFUN1,	SYS_RREF	},
	{ "ts",			APL_VARSYS,		SYS_TS		},
	{ "ver",		APL_VARSYS,		SYS_VER		},
	{ "wsid",		APL_VARSYS,		SYS_WSID	}
};

int nSysNames = sizeof(SysNames) / sizeof(SysNames[0]);

static int	CharUTF8(LEXER *plex, char *buf, int len);
static void EmitArray(LEXER *plex);
static void EmitName(LEXER *plex);
static void EmitString(LEXER *plex);
static void EmitSysName(LEXER *plex);
static void TokExponent(LEXER *plex);
static void TokFraction(LEXER *plex);
static int  TokInteger(LEXER *plex);
static void TokName(LEXER *plex);
static void TokNumber(LEXER *plex);
static void TokString(LEXER *plexid);
static void TokSysName(LEXER *plex);
static void UTF8Char(LEXER *plex);

/* Initialize lexer object
   The source is already in the buffer
      1 line -> REPL or edit buffer
      Multiple lines -> FUNCTION

/----------- srclen -----------\
+-----------------------------+-+-------+-----------+--------------------------+
| source line(s)              |0| align | line offs | literals -->    <-- code |
+-----------------------------+-+-------+-----------+--------------------------+
 ^                               ^       ^           ^                        ^
 psrcBase                  psrcEnd       plinBase    plitBase                 pobjBase

 Line offsets table (nlines + 1)

                Source   Object
		      +--------+--------+
 plinBase --> | Offset | Offset | Line 0 (header)
              +--------+--------+
			  | Offset | Offset | Line 1
			  +--------+--------+
			  |       ...       |
			  +--------+--------+
			  | Offset | Offset | Line n
			  +--------+--------+


*/

// Initialize the lexer to use an external line buffer
void CreateLexer(LEXER *plex, char *buffer, int buflen, int nlines, char *pnames)
{
	plex->psrcBase = buffer;
	plex->buflen = buflen;
	plex->nlines = nlines;
	plex->pnameBase = pnames;
	plex->pobjBase = buffer + buflen - 1;	// Last byte in buffer
}

// Prepare lexer for a new source line
void InitLexer(LEXER *plex, int srclen)
{
	char *buffer;

	buffer = plex->psrcBase;

	plex->psrcEnd = buffer + srclen;
	plex->pChr = buffer;
	plex->pexprBase = buffer;
	plex->fInQuotes = 0;

	plex->plinBase = (offset *)ALIGN_UP(plex->psrcEnd, sizeof(double));

	InitLexerAux(plex);
}

void InitLexerAux(LEXER *plex)
{
	plex->plitBase = (double *)(plex->plinBase + (plex->nlines + 1) * 2);
	if ((char *)plex->plitBase > plex->pobjBase) {
		print_line("Too many lines in function");
		exit(1);
	}

	plex->plitTop = plex->plitBase;
	plex->litIndx = 0;
	plex->pCode = plex->pobjBase;
	*plex->pCode-- = APL_END;

	// Get first token if reading from a single line (e.g. the REPL).
	// Don't do this when reading from a function because the first
	// byte points to the line length, not the source beginning.
	// CompileFun() knows how to deal with this.
	if (!plex->nlines && !plex->pnameBase) {
		NextChr(plex);
		NextTok(plex);
	}
}

void NextChr(LEXER *plex)
{
	if (plex->pChr >= plex->psrcEnd) {
		plex->lexChr = 0;
		return;
	}

	plex->pChrBase = plex->pChr;
	plex->lexChr = *plex->pChr;

	if (plex->lexChr < 128)
		++plex->pChr;		// 1 char = 1 byte
	else {					// 1 char = 1 to 4 bytes (UTF-8)
		int len;
		int wchr = utf8_to_wchar(plex->pChr, plex->psrcEnd - plex->pChr - 1, &len);
		if (wchr) {
			plex->lexChr = wchr;
			plex->pChr += len;
		} else
			LexError(plex,LE_BAD_TOKEN);
	}
}

void NextTok(LEXER *plex)
{
	// Skip blanks
	while (plex->lexChr == ' ' || plex->lexChr =='\t' || plex->lexChr == '\n')
		NextChr(plex);

	// Remember beginning of token
	plex->ptokBase = plex->pChrBase;

	int chr = plex->lexChr;
	if (!chr) {
		plex->tokTyp = APL_END;
		return;
	}

	if (IsNumber(chr))
		TokNumber(plex);
	else if (chr =='\'')
		TokString(plex);
	else if (isalpha(chr) || chr == CHAR_DELTA || chr == '_')
		TokName(plex);
	else {
		plex->tokTyp = token_from_char(chr);
		if (!plex->tokTyp)
			LexError(plex,LE_BAD_TOKEN);
		
		NextChr(plex);
		// System variable/function?
		if (plex->tokTyp == APL_QUAD && isalpha(plex->lexChr))
			TokSysName(plex);
	}
}

// Invert the order of diamond-separated list of expressions
// From:  exprn ⋄ ... ⋄ expr2 ⋄ expr1
// To:    expr1 ⋄ expr2 ⋄ ... ⋄ exprn
static void TokSubExprs(LEXER *plex)
{
	size_t len = plex->pobjBase - plex->pCode;
	char *buf = malloc(len);
	char *end = buf + len - 1;
	char *src, *dst;

	// Copy original code sequence to a temp buffer
	memcpy(buf, plex->pCode + 1, len);
	src = buf;
	dst = plex->pobjBase;

	// Each sub-expression is preceded by its length
	while (src < end) {
		len = *src++;
		dst -= len;
		memcpy(dst, src, len);
		src += len;
		*--dst = APL_DIAMOND;
	}

	// Skip last diamond
	++plex->pCode;

	free(buf);
}

int TokExpr(LEXER *plex)
{
	char *pdiam = plex->pobjBase;	// Points to the last diamond

	PUSHJUMP();
	if (SETJUMP()) {
		POPJUMP();
		return 0;
	}

	while (plex->tokTyp != APL_END) {
		switch (plex->tokTyp) {
		case APL_NUM:
			EmitArray(plex);
			break;
		case APL_STR:
			EmitString(plex);
			NextTok(plex);
			break;
		case APL_VARNAM:
			EmitName(plex);
			NextTok(plex);
			break;
		case APL_VARSYS:
		case APL_SYSFUN1:
			EmitSysName(plex);
			NextTok(plex);
			break;
		case APL_LAMP:		// ⍝ comment
			// Ignore rest of line
			plex->tokTyp = APL_END;
			plex->lexChr = 0;
			break;
		case APL_DIAMOND:	// ⋄ expression separator
			EmitTok(plex,pdiam-plex->pCode-1); // Length of sub-expression
			pdiam = plex->pCode + 1;
			NextTok(plex);
			break;
		default:
			if (!IsToken(plex->tokTyp))
				LexError(plex,LE_BAD_TOKEN);
			EmitTok(plex,plex->tokTyp);
			NextTok(plex);
			break;
		}
	}

	// If the line contains diamond-separated sub-expressions, they need to be inverted.
	// Although an expression is evaluated from right to left, each sub-expression
	// is evaluated from left to right.
	if (pdiam != plex->pobjBase) {
		EmitTok(plex,pdiam-plex->pCode-1); // Length of last sub-expression
		TokSubExprs(plex);
	}

	POPJUMP();
	return 1;
}

static void TokNumber(LEXER *plex)
{
	int sign = 0;

	// Is this '.' the beginning of a decimal number
	// or the dot operator?
	if (plex->lexChr == '.' && !isdigit(*plex->pChr)) {
		plex->tokTyp = plex->lexChr;  // Operator
		NextChr(plex);
		return;
	}

	// Is there room for another literal?
	if ((char *)(plex->plitTop + 1) >= plex->pCode)
		LexError(plex,LE_TOO_MANY_LITERALS);

	if (plex->lexChr == CHAR_HIGHMINUS) {
		sign = 1;
		NextChr(plex);
		if (!isdigit(plex->lexChr) && plex->lexChr != '.')
			LexError(plex,LE_BAD_NUMBER);
	}

	plex->tokTyp = APL_NUM;

	// At least integer part or fraction must be present
	if (plex->lexChr == '.') {
		NextChr(plex);
		if (!isdigit(plex->lexChr))
			LexError(plex,LE_BAD_NUMBER);
		plex->tokNum = 0.0;
		TokFraction(plex);
	} else {
		plex->tokNum = (double)TokInteger(plex);
		if (plex->lexChr == '.') {
			NextChr(plex);
			TokFraction(plex);
		}
	}

	if (plex->lexChr == 'E'  ||  plex->lexChr == 'e')
		TokExponent(plex);

	if (sign)
		plex->tokNum = -plex->tokNum;

	plex->plitBase[plex->litIndx++] = plex->tokNum;
	++plex->plitTop;
}

static int TokInteger(LEXER *plex)
{
	int val = 0;

	while (isdigit(plex->lexChr)) {
		val = val * 10 + (plex->lexChr - '0');
		NextChr(plex);
	}

	return val;
}

static void TokFraction(LEXER *plex)
{
	double pow10 = 0.1;

	while (isdigit(plex->lexChr)) {
		plex->tokNum = plex->tokNum + pow10 * (plex->lexChr - '0');
		pow10 /= 10.0;
		NextChr(plex);
	} while (isdigit(plex->lexChr));

}

static void TokExponent(LEXER *plex)
{
	int expo = 0;
	int sign = 0;

	NextChr(plex);

	if (plex->lexChr == CHAR_HIGHMINUS) {
		sign = 1;
		NextChr(plex);
		if (!isdigit(plex->lexChr))
			LexError(plex,LE_BAD_NUMBER);
	} else if (plex->lexChr == '+')
		NextChr(plex);

	if (!isdigit(plex->lexChr))
		LexError(plex,LE_BAD_NUMBER);

	do {
		expo = expo * 10 + (plex->lexChr - '0');
		NextChr(plex);
	} while (isdigit(plex->lexChr));

	if (sign)
		expo = -expo;

	plex->tokNum = plex->tokNum * pow(10,expo);
}

static void TokString(LEXER *plex)
{
	char *pch;
	int len = 0;

	pch = plex->tokStr;
	plex->fInQuotes = 1;
	while (1) {
		NextChr(plex);
		if (plex->lexChr == '\'') {
			plex->fInQuotes = 0;
			NextChr(plex);
			if (plex->lexChr != '\'')
				break;
			plex->fInQuotes = 1;
		}
		if (plex->lexChr == APL_END)
			LexError(plex,LE_BAD_STRING);


		if (plex->lexChr < 128) {	// ASCII
			if (++len >= STRINGMAXSIZ)
				LexError(plex,LE_STRING_TOO_LONG);
			*pch++ = plex->lexChr;
		} else {					// Unicode
			int n = wchar_to_utf8(plex->lexChr, pch, STRINGMAXSIZ - len);
			if (!n)
				LexError(plex,LE_STRING_TOO_LONG);
			len += n;
			pch += n;
		}
	}

	plex->tokLen = len;
	plex->tokTyp = APL_STR;
}

void TokPrint(char *base, double *litBase)
{
	char token[16];
	char *pc = base;
	int n, tok;
	double *pdbl;

	while (*pc != APL_END) {
		tok = *pc;
		print_line("%04d %03d ",pc - base, tok);
		switch (tok) {
		case APL_NUM:
			tok = *++pc;
			print_line("NUM=%g\n", litBase[tok]);
			break;

		case APL_CHR:
			tok = *++pc;
			print_line("CHR='%C'\n", tok);
			break;
	
		case APL_ARR:
			print_line("ARR=");
			n = *++pc;
			tok = *++pc;
			pdbl = litBase + tok;
			while (n--)
				print_line("%g ", *pdbl++);
			print_line("\n");
			break;

		case APL_STR:
			n = *++pc;
			print_line("STR=%.*s\n", n, pc+1);
			pc += n;
			break;

		case APL_VARNAM:
			++pc;
			tok = *pc++;
			print_line("VARNAM %*.*s (L=%d)\n", tok, tok, pc, tok);
			pc += tok - 1;
			break;

		case APL_VARINX:
			tok = *++pc;
			print_line("VARINX I=%d\n", tok);
			break;

		case APL_VARSYS:
			tok = *++pc;
			print_line("VARSYS I=%d\n", tok);
			break;

		case APL_SYSFUN1:
			tok = *++pc;
			print_line("SYSFUN1 I=%d\n", tok);
			break;

		case APL_SYSFUN2:
			tok = *++pc;
			print_line("SYSFUN2 I=%d\n", tok);
			break;

		case APL_NL:
			print_line("NL\n");
			break;

		default:
			n = wchar_to_utf8(AplTokens[tok].code, token, sizeof(token)-1);
			token[n] = 0;
			print_line("%s\n", token);
			break;
		}
		++pc;
	}
	print_line("%04d 000 END\n",pc - base);
}

static void TokSysName(LEXER *plex)
{
	char name[NAMEMAXSIZ+1];
	char *base;
	int len;

	base = plex->ptokBase;

	// System names are case-insensitive
	for (len = 0; isalpha(plex->lexChr); ++len) {
		if (len >= NAMEMAXSIZ || plex->lexChr > 127)
			LexError(plex, LE_BAD_SYSTEM_NAME);
		name[len] = tolower(plex->lexChr);
		NextChr(plex);
	}
	name[len] = 0;

	// Quick and dirty linear search
	// Make this a better map when the number of names increases
	NameIndex *psn = SysNames;
	for (int i = 0; i < nSysNames; ++i, ++psn)
		if (!strcmp(psn->pName, name)) {
			plex->tokTyp = psn->token;
			plex->tokAux = psn->index;
			plex->tokLen = plex->pChrBase - plex->ptokBase;
			return;
		}

	LexError(plex, LE_BAD_SYSTEM_NAME);
}

static void TokName(LEXER *plex)
{
	while (1) {
		NextChr(plex);
		if (!isalnum(plex->lexChr) && plex->lexChr != CHAR_DELTA && plex->lexChr != '_')
			break;
	}

	plex->tokTyp = APL_VARNAM;
	plex->tokLen = plex->pChrBase - plex->ptokBase;
	if (plex->tokLen > NAMEMAXSIZ)
		LexError(plex,LE_BAD_NAME);
}

static void EmitArray(LEXER *plex)
{
	int n, indx;

	/*
	** Scalar: [APL_NUM] [INDX]
	** Vector: [APL_ARR] [DIMS] [INDX]
	*/

	indx = plex->litIndx - 1;

	while (plex->tokTyp == APL_NUM)
		NextTok(plex);

	EmitTok(plex,indx);

	if ((n = plex->litIndx - indx) > 1)	// Vector
	{
		EmitTok(plex,n);
		EmitTok(plex,APL_ARR);
	}
	else							/* Scalar */
		EmitTok(plex,APL_NUM);
}

static void EmitName(LEXER *plex)
{
	char *pch;

	/*
	** Variable by name:    [APL_VARNAM] [LEN] [LEN CHARS...]
	** Variable by index:   [APL_VARINX] [INX]
	*/

	if (plex->pnameBase && (pch = FindName(plex->pnameBase, plex->ptokBase, plex->tokLen))) {
		if (pch[1] == FUN_LAB) {
			// Replace label with corresponding line number
			EmitNumber(plex, (double)pch[2]);
			return;
		}
		
		if (pch[1] != FUN_NAM) {
			// Local variable (index)
			plex->pCode -= 2;
			if (plex->pCode < (char *)plex->plitTop)
				LexError(plex,LE_CODE_FULL);
			*(plex->pCode + 2) = pch[2];	// Index in name table
			*(plex->pCode + 1) = APL_VARINX;
			return;
		}

		// FUN_NAM (recursive calls are handled by APL_VARNAM)
	} 
	
	// Global variable (name)
	plex->pCode -= plex->tokLen + 2;
	if (plex->pCode < (char *)plex->plitTop)
		LexError(plex,LE_CODE_FULL);
	*(plex->pCode + 2) = plex->tokLen;
	memcpy(plex->pCode + 3, plex->ptokBase, plex->tokLen);
	*(plex->pCode + 1) = APL_VARNAM;
}

static void EmitSysName(LEXER *plex)
{
	/*
	** System variable: [APL_VARSYS] [INX]
	*/

	plex->pCode -= 2;
	if (plex->pCode < (char *)plex->plitTop)
		LexError(plex,LE_CODE_FULL);
	*(plex->pCode + 2) = plex->tokAux;
	*(plex->pCode + 1) = plex->tokTyp;
}

static void EmitString(LEXER *plex)
{
	if (plex->tokLen == 1) {
		// Character: [APL_CHR] [CHAR]
		plex->pCode -= 2;
		if (plex->pCode < (char *)plex->plitTop)
			LexError(plex,LE_CODE_FULL);
		*(plex->pCode + 1) = APL_CHR;
		*(plex->pCode + 2) = *plex->tokStr;
	} else {
		// String: [APL_STR] [LEN] [LEN CHARS...]
		plex->pCode -= plex->tokLen + 2;
		if (plex->pCode < (char *)plex->plitTop)
			LexError(plex,LE_CODE_FULL);
		*(plex->pCode + 1) = APL_STR;
		*(plex->pCode + 2) = plex->tokLen;
		memcpy(plex->pCode + 3, plex->tokStr, plex->tokLen);
	}
}

void EmitTok(LEXER *plex, int tok)
{
	if (plex->pCode < (char *)plex->plitTop)
		LexError(plex,LE_CODE_FULL);

	*plex->pCode-- = tok;
}

void EmitNumber(LEXER *plex, double num)
{
	/*
	** Scalar: [APL_NUM] [INDX]
	*/

	// Is there room for another literal?
	if ((char *)(plex->plitTop + 1) >= plex->pCode)
		LexError(plex,LE_TOO_MANY_LITERALS);

	plex->plitBase[plex->litIndx++] = num;
	++plex->plitTop;

	EmitTok(plex,plex->litIndx - 1);
	EmitTok(plex,APL_NUM);
}

void LexError(LEXER *plex,int errnum)
{
	PutErrorLine("\n%s\n", plex->pexprBase);

	if (plex->pChrBase >= plex->pexprBase && plex->pChrBase < (char *)plex->pobjBase) {
		int i = plex->pChrBase - plex->pexprBase;
		while (i--)
			PutErrorChar(' ');
		PutErrorLine("^\n");
	}

	PutErrorLine("[LexicalError] %s\n", apchLexMsg[errnum]);

	LONGJUMP();
}

/* EOF */
