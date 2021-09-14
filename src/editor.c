// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <string.h>

#include "apl.h"
#include "error.h"
#include "token.h"

static char  *EditLine(char *pLine, int nLen, int nPos, int nLin);

static jmp_buf jbEditMode;

char *apchEdtMsg[] =
{
	"No error",					/* 0 */
	"Function too big",			/* 1 */
	"Invalid line number",		/* 2 */
	"Invalid editor command",	/* 3 */
	"Invalid function header"	/* 4 */
};

/*
 * ∇ enters definition mode (the function editor)
 * ∇ exits definition mode and goes back to execution mode
 * 
 * Line 0 is the function header
 */


/*
 * Possible editor commands:
 * Warning: some of these commands may behave differently in other APL editors!
 * 
 * ∇ fun [⎕]       Display all lines, insert after end
 * ∇ fun [N⎕]      Display from line N to end, insert after end
 * ∇ fun [⎕N]      Display from line 1 to line N, insert after line N
 * ∇ fun [M⎕N]     Display from line M to line N, insert after line N
 * ∇ fun [N]       Replace line N, N+1, ...
 * ∇ fun [∆N]      Delete line N, insert at that point
 */

// Make sure nCurLin is valid (1 <= nCurLin <= nLines + 1)
// If the line is invalid, sanitize it and ignore rest of input
#define	CHECK_LINE()	if (nCurLin < 1 || nCurLin > pfun->nLines + 1) {\
							nCurLin = pfun->nLines + 1;					\
							plex->tokTyp = APL_END;						\
							EdtError(DE_BAD_LINE_NUMBER);				\
						}

void EditFun(FUNCTION *pfun, LEXER *plex)
{
	LEXER lex;
	int len, n, nCurLin, nSrcMax, fEnd, fDel;
	int line1, line2;
	char *pIns, *pEnd, *pSrc;
	char *pfunBase;

	nSrcMax = pfun->nFunSiz - pfun->oSource;
	pfunBase = POINTER(pfun,pfun->oSource);

	// Start inserting after last line
	nCurLin = pfun->nLines + 1;
	pEnd = pfunBase + pfun->nSrcSiz;
	pSrc = plex->psrcBase;
	fDel = 0;	// True if current line is going to be replaced

	PUSHJUMP();
	SETJUMP();

readline:
	// If we have not reached the end of the current line, keep parsing
	if (plex->tokTyp != APL_END) goto parseline;

	// Need to get a new line
	do {
		n = print_line("[%d]", nCurLin);
		print_line(g_blanks + n);
		len = GetLine(plex->psrcBase, plex->buflen);
	} while (len == 0);
	if (len < 0) {
		// EOF
		POPJUMP();
		return;
	}
	pSrc = plex->psrcBase;
	InitLexer(plex, len + 1);

parseline:
	fEnd = 0;		// True if line ends with ∇

	line1 = line2 = 0;
	if (plex->tokTyp == APL_LEFT_BRACKET) {
		// Display/edit command
		NextTok(plex);

		if (plex->tokTyp == APL_LESS_THAN) {		// Insert before line [< {line}]
			NextTok(plex);
			if (plex->tokTyp == APL_NUM) {
				nCurLin = (int)plex->tokNum;
				CHECK_LINE();
				NextTok(plex);
			} else
				nCurLin = 1;
		}
		else if (plex->tokTyp == APL_GREATER_THAN) {	// Insert after line [> {line}]
			NextTok(plex);
			if (plex->tokTyp == APL_NUM) {
				nCurLin = (int)plex->tokNum + 1;
				CHECK_LINE();
				NextTok(plex);
			} else
				nCurLin = pfun->nLines + 1;
		}
		else if (plex->tokTyp == APL_DELTA) { // Delete line [∆ {line}]
			char *pDel;

			NextTok(plex);
			// If line # is omitted, delete current line
			if (plex->tokTyp == APL_NUM) {
				nCurLin = (int)plex->tokNum;
				CHECK_LINE();
				NextTok(plex);
			}
			// Position deletion point at the beginning of nCurLin
			// Assume 1 <= nCurLin <= nLines + 1
			for (pDel = pfunBase, n = 0; n < nCurLin; ++n)
				pDel += *pDel + 2;

			if (pDel != pEnd) {
				// Delete this line
				n = *pDel + 2;
				memmove(pDel, pDel + n, pEnd - pDel - n);
				pEnd -= n;
				--pfun->nLines;
				pfun->nSrcSiz -= n;
			}
		} else {	// Display commands [{line1} {⎕} {line2}]
			// Prohibit []
			n = 0;
			if (plex->tokTyp == APL_NUM) {
				n = 1;
				line1 = (int)plex->tokNum;
				NextTok(plex);
				if (plex->tokTyp == APL_RIGHT_BRACKET) {
					nCurLin = line1;
					CHECK_LINE();
					fDel = 1;		// Replace this line with new one
				}
			}
			if (plex->tokTyp == APL_QUAD) {
				n = 1;
				NextTok(plex);
				if (plex->tokTyp == APL_NUM) {
					if (!line1) line1 = 1;
					line2 = (int)plex->tokNum;
					NextTok(plex);
				} else {
					if (!line1) line1 = ALL_LINES;
					line2 = pfun->nLines;
				}
				nCurLin = line2 + 1;
				CHECK_LINE();
				fDel = 0;
			}
			if (!n)
				EdtError(DE_BAD_EDIT_CMD);
		}
		if (plex->tokTyp != APL_RIGHT_BRACKET || !n)
			EdtError(DE_BAD_EDIT_CMD);
		NextTok(plex);
		pSrc = plex->ptokBase;
	}

	while (plex->tokTyp != APL_END) {
		if (fEnd) // Nothinhg should follow the ∇
			EdtError(DE_BAD_EDIT_CMD);
		if (plex->tokTyp == APL_DEL) {
			// End editing
			// Replace APL_DEL with APL_END and flag end
			*plex->ptokBase = APL_END;
			fEnd = 1;
		}
		NextTok(plex);
	}

	// Display lines?
	if (line1 && line2)
		PrintFun(pfun, line1, line2, 0);

	// Insert new line?
	if ((len = strlen(pSrc))) {
		// Position insertion point at the beginning of nCurLin
		// Assume 1 <= nCurLin <= nLines + 1
		for (pIns = pfunBase, n = 0; n < nCurLin; ++n)
			pIns += *pIns + 2;

		if (fDel && pIns != pEnd) {
			// Delete current line before inserting the new one
			n = *pIns + 2;
			memmove(pIns, pIns + n, pEnd - pIns - n);
			pEnd -= n;
			--pfun->nLines;
			pfun->nSrcSiz -= n;
		}
		fDel = 0;

		// Now insert new line
		if ((pfun->nSrcSiz += len + 2) > nSrcMax) {
			// Not enough room
			pfun->nSrcSiz -= len + 2;
			EdtError(DE_FUNCTION_TOO_BIG);
		}
		// Make room for new line
		if (pIns != pEnd)
			memmove(pIns + len + 2, pIns, pEnd - pIns);
		pEnd += len + 2;

		// Copy new line to edit buffer, including terminator
		*pIns = len;
		memcpy(pIns + 1, pSrc, len + 1);
		++pfun->nLines;
		++nCurLin;
		pfun->fDirty = 1;
	}

	if (!fEnd) goto readline;

	if (pfun->fDirty) {
		// Create new lexer for the full function
		CreateLexer(&lex, pfunBase, nSrcMax, pfun->nLines, &pfun->aNames[0]);
		InitLexer(&lex, pfun->nSrcSiz);

		// Compile and save the funtion
		CompileFun(pfun, &lex);
		SaveFun(pfun, &lex);
	}

	POPJUMP();
}

#define	MAXLIN	70
static char *EditLine(char *pLine, int nLen, int nPos, int nLin)
{
	static char achLine[MAXLIN + 1];
	int i, ch, fIns;

	// Start in insert mode
	fIns = 1;
	if (nLen > MAXLIN)
		return NULL;
	memcpy(achLine, pLine, nLen);
	achLine[nLen] = '\0';
	/*
	   Valid cursor positions (nPos):
	      
		  0 <= nPos < nLen - Existing characters
		  nPos = nLen      - After the last character

       On input nPos = 0 means nLen.
	*/
	if (nPos < 0)
		nPos = 0;
	else if (!nPos)
		nPos = nLen;
	else if (nPos >= nLen)
		nPos = nLen;
	else
		--nPos;

	print_line("<%d>\t%s", nLin, achLine);
	for (i = nLen - nPos; i > 0; --i)
		PutChar('\b');

	while (1) {
		ch = GetChar();
		switch (ch) {
		case CHAR_BS:
			if (!nPos) {
				Beep();
				break;
			}
			--nPos;
			PutChar(CHAR_BS);
			// Fall through CHAR_DEL
		case CHAR_DEL:
			if (nPos < nLen) {
				// Move left (including terminator)
				memmove(&achLine[nPos], &achLine[nPos+1], nLen - nPos);
				--nLen;
				// Print right part + 1 blank
				print_line("%s ", achLine + nPos);
				for (i = nLen - nPos + 1; i > 0; --i)
					PutChar('\b');
			}
			else Beep();
			break;
		case CHAR_HOME:
			while (nPos) {
				PutChar(CHAR_BS);
				--nPos;
			}
			break;
		case CHAR_END:
			while (nPos < nLen) {
				PutChar(achLine[nPos]);
				++nPos;
			}
			break;
		case CHAR_LEFT:
			if (nPos) {
				PutChar(CHAR_BS);
				--nPos;
			} else Beep();
			break;
		case CHAR_RIGHT:
			if (nPos < nLen) {
				PutChar(achLine[nPos]);
				++nPos;
			} else Beep();
			break;
		case CHAR_INS:
			// Toggle insert mode
			fIns = !fIns;
			break;
		case CHAR_UP:
		case CHAR_DOWN:
		case CHAR_PG_UP:
		case CHAR_PG_DN:
			Beep();
			continue;
		case CHAR_CR:
			PutChar('\n');
			return achLine;
		default:
			if (fIns) {
				if (nLen >= MAXLIN) {
					Beep();
					continue;
				}
				// Move right (including terminator)
				memmove(&achLine[nPos+1], &achLine[nPos], nLen - nPos + 1);
				// Print new character + right part
				achLine[nPos] = ch;
				print_line("%s", achLine + nPos);
				++nLen;
				++nPos;
				for (i = nLen - nPos; i > 0; --i)
					PutChar('\b');
			} else {
				if (nPos < nLen) {
					achLine[nPos++] = ch;
					PutChar(ch);
				} else {
					if (nLen >= MAXLIN) {
						Beep();
						continue;
					}
					achLine[nPos++] = ch;
					achLine[nPos] = '\0';
					PutChar(ch);
					++nLen;
				}
			}
			break;
		}
	}
	return NULL;	// Keep the compiler happy!
}

void EdtError(int errnum)
{
	print_line("[EditError] %s\n", apchEdtMsg[errnum]);

	LONGJUMP();
}
