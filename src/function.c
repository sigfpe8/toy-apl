// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <string.h>

#include "apl.h"
#include "error.h"
#include "token.h"

static void	  AddLabel(LEXER *plex, FUNCTION *pfun, int line);
static int    CopyNames(char *pdst, char *psrc);
static void   SwapNames(char *base, char *pfun);

void NewFun(LEXER *plex)
{
	char EditBuffer[2048];
	FUNCTION *pfun;
	char *pfunBase;
	int len;

	pfun = (FUNCTION *)EditBuffer;
	memset(pfun, 0, sizeof(FUNCTION));
	pfun->oSource = sizeof(FUNCTION) + 256;
	pfun->nFunSiz = sizeof(EditBuffer);
	pfunBase = POINTER(pfun,pfun->oSource);
	len = plex->psrcEnd - plex->psrcBase; // Includes EOS

	InitLexer(plex, len);
	ParseHeaderFun(pfun, plex);

	// Copy header from lexer to function
	*pfunBase = len;
	memcpy(pfunBase + 1, plex->psrcBase, len);
	pfun->nSrcSiz = len + 2;
	pfun->nLines = 0;
	pfun->fDirty = 1;

	// plex->tokTyp == APL_END
	EditFun(pfun, plex);
}

void OpenFun(LEXER *plex, VNAME *pn)
{
	char EditBuffer[2048];
	FUNCTION *pfold, *pfnew;
	char *psrc, *pdst;
	DESC *pd;
	int n;

	pd = (DESC *)WKSPTR(pn->odesc);
	pfold = (FUNCTION *)WKSPTR(VOFF(pd));
	pfnew = (FUNCTION *)EditBuffer;

	// Copy header to edit buffer
	memcpy(pfnew, pfold, sizeof(FUNCTION));
	// Copy variable names (but not labels)
	n = CopyNames(&pfnew->aNames[0], &pfold->aNames[0]);
	pfnew->nHdrSiz = sizeof(FUNCTION) + n - 2;

	pfnew->oSource = sizeof(FUNCTION) + 256;
	pfnew->nFunSiz = sizeof(EditBuffer);
	pdst = POINTER(pfnew,pfnew->oSource);
	psrc = POINTER(pfold,pfold->oSource);

	// Copy source to edit buffer
	memcpy(pdst, psrc, pfold->nSrcSiz);
	pfnew->fDirty = 0;
	pfnew->nLits = 0;
	pfnew->nObjSiz = 0;
	pfnew->oObject = 0;

	// plex->tokTyp == '['
	EditFun(pfnew, plex);
}

void SaveFun(FUNCTION *pfun, LEXER *plex)
{
	offset off, onew;
	offset osrc, oobj;
	offset snames, slits, slins, ssrc, sobj;
	char *pdst;
	FUNCTION *pnew;
	DESC *pd;

	// Edit buffer before compilation:
	//   header - names (gap) source
	// Edit buffer after compilation:
	//   header - names (gap) source (align) line offsets (align) literals (gap) object
	// Function structure:
	//   header - names (align) literals - line offsets - source - object

	off = ALIGN_UP(pfun->nHdrSiz, sizeof(double));
	snames = off;

	// Literals (can be absent)
	slits = pfun->nLits * sizeof(double); // Can be zero
	off += slits;

	// Line offsets (at least 1+1)
	slins = (pfun->nLines + 1) * 2 * sizeof(offset);
	off += slins;

	// Source
	osrc = off;
	ssrc = pfun->nSrcSiz;
	off += ssrc;

	// Object
	oobj = off;
	sobj = pfun->nObjSiz;
	off += sobj;

	// Allocate new space
	onew = AplHeapAlloc(off, 0);
	pnew = WKSPTR(onew);
	pdst = (char *)pnew;
	// Copy header + names
	memcpy(pdst, (char *)pfun, snames);
	pdst += snames;
	// Copy literals
	memcpy(pdst, (char *)plex->plitBase, slits);
	pdst += slits;
	// Copy line offsets
	memcpy(pdst, (char *)plex->plinBase, slins);
	pdst += slins;
	// Copy source code
	memcpy(pdst, plex->psrcBase, ssrc);
	pdst += ssrc;
	// Copy object code
	memcpy(pdst, plex->pCode, sobj);

	// Fix offsets, etc.
	pnew->nFunSiz = off;
	pnew->nHdrSiz = snames;
	pnew->oSource = osrc;
	pnew->oObject = oobj;
	pnew->fDirty = 0;

	pd = GlobalDescAlloc();
	TYPE(pd) = TFUN + pfun->nArgs;
	VOFF(pd) = onew;
	// The function name is the 1st name in the names table
	SetName(pfun->aNames[0], &pfun->aNames[0]+3, pd);

	if (DEBUG_FLAG(DBG_DUMP_FUNCTION))
		DumpFun(pnew);
}

void DumpFun(FUNCTION *pfun)
{
	static char *types[] = {
		"FUN",	// 0 -> Function name
		"RET",  // 1 -> Return value
		"ARG",  // 2 -> Function argument
		"LOC",  // 3 -> Local variables
		"LAB",  // 4 -> Function labels
		"GLB"	// 5 -> Global variables
	};
	int i, len;
	char   *pch;
	double *plit;
	offset *plin;
	uchar  *pcod;
	int		hdrsz = 40;

	PutDashLine(hdrsz, "--- FUNCTION at %p ---\n", pfun);
	print_line("nFunSiz = %d\n", pfun->nFunSiz);
	print_line("nHdrSiz = %d\n", pfun->nHdrSiz);
	print_line("nSrcSiz = %d\n", pfun->nSrcSiz);
	print_line("nObjSiz = %d\n", pfun->nObjSiz);

	print_line("oSource = 0x%x\n", pfun->oSource);
	print_line("oObject = 0x%x\n", pfun->oObject);

	print_line("nLines  = %d\n", pfun->nLines);
	print_line("nLits   = %d\n", pfun->nLits);
	print_line("nArgs   = %d\n", pfun->nArgs);
	print_line("nLocals = %d\n", pfun->nLocals);
	print_line("nRet    = %d\n", pfun->nRet);

	plit = POINTER(pfun,pfun->nHdrSiz);
	plin = POINTER(plit,pfun->nLits * sizeof(double));
	pcod = POINTER(pfun,pfun->oObject);

	PutDashLine(hdrsz, "--- Names -\n");
	pch = &pfun->aNames[0];
	while ((len = *pch)) {
		print_line("%6.*s T=%s, I=%d\n", len, pch+3, types[pch[1]], pch[2]);
		pch += len + 3;
	}

	PutDashLine(hdrsz, "--- Index  Literal -\n");
	for (i = 0; i < pfun->nLits; ++i)
		print_line("    % 4d    % 6g\n", i, plit[i]);

	PutDashLine(hdrsz, "--- Line  Source  Object -\n");
	for (i = 0; i <= pfun->nLines; ++i)
		print_line("    % 4d    %04d    %04d\n", i, plin[i*2], plin[i*2+1]);

	PutDashLine(hdrsz, "--- Source -\n");
	PrintFun(pfun, ALL_LINES, 0, 1);

	PutDashLine(hdrsz, "-- Object -\n");
	TokPrint(POINTER(pfun,pfun->oObject),POINTER(pfun,pfun->nHdrSiz));
	PutDashLine(hdrsz, "---\n");
}

void CompileFun(FUNCTION *pfun, LEXER *plex)
{
	int base, i;
	char *pch;

	/*
	    On entry:
           /--L0--\     /--L1--\           /--Ln--\
		+-+--------+-+-+--------+-+-----+-+--------+-+--------------------------------+
		|L| Line 0 |0|L| Line 1 |0| ... |L| Line n |0|                                |
		+-+--------+-+-+--------+-+-----+-+--------+-+--------------------------------+
		 ^                                            ^                              ^
		 psrcBase                               psrcEnd                              pobjBase

		Each line begins with its length and ends with 0
	*/

	// Create line offsets table
	// Source line 0 has offset 0
	pch = plex->psrcBase;
	for (i = 0; i <= pfun->nLines; ++i) {
		SRC_LINEOFF(plex,i) = OFFSET(plex->psrcBase,pch);
		pch += *pch + 2; // L + 0
	}

	// Scan all lines looking for labels
	for (i = 1; i <= pfun->nLines; ++i) {
		plex->pChr = plex->pexprBase = (char *)POINTER(plex->psrcBase,SRC_LINEOFF(plex,i)) + 1;
		NextChr(plex);
		NextTok(plex);
		// Is this line started with LABEL: ?
		if (plex->tokTyp == APL_VARNAM && plex->lexChr == ':')
			AddLabel(plex, pfun, i);
	}

	/* Compile the funtion backwards, from last line to first
	   Source:
	      Line 1: a b c
	      Line 2: d e f
		  ...
		  Line n: x y z
	  
	   Object:
	      c b a APL_NL f e d APL_NL ... z y x APL_NL APL_END

		  Each line ends with either APL_NL (new line) or APL_RIGHT_ARROW (goto)
		  The function ends with APL_END
	*/

	for (i = pfun->nLines; i > 0; --i) {
		plex->pChr = plex->pexprBase = (char *)POINTER(plex->psrcBase,SRC_LINEOFF(plex,i)) + 1;
		NextChr(plex);
		NextTok(plex);
		// Is this line started with LABEL: ?
		if (plex->tokTyp == APL_VARNAM && plex->lexChr == ':') {
			NextTok(plex);	// Skip label
			NextTok(plex);	// Skip :
		}
		// Is this a branch?
		if (plex->tokTyp != APL_RIGHT_ARROW) {
			// If not a branch, insert APL_NL token
			EmitTok(plex,APL_NL);
		}
		TokExpr(plex);
		// Temporary offset, relative to the beginning of the function header
		OBJ_LINEOFF(plex,i) = OFFSET(pfun,plex->pCode + 1);
	}
	pfun->nObjSiz = plex->pobjBase - plex->pCode;
	pfun->nLits = plex->litIndx;
	++plex->pCode;

	// Rebase object code offsets relative to the beginning of the object code
	// Object line 1 has offset 0 (there's no object code for line 0)
	base = OBJ_LINEOFF(plex,1);
	pfun->oObject = base;
	OBJ_LINEOFF(plex,0) = 0;
	for (i = 1; i <= pfun->nLines; ++i)
		OBJ_LINEOFF(plex,i) = OBJ_LINEOFF(plex,i) - base;
}

void FPrintFun(FILE *pf, FUNCTION *pfun)
{
	int i;
	char *plin;

	// Header
	plin = (char *)pfun + pfun->oSource;
	fprintf(pf, "%s\n", plin + 1);
	plin += *plin + 2;

	// Function boddy
	for (i = 1; i <= pfun->nLines; ++i)
	{
		fprintf(pf, "  %s\n", plin + 1);
		plin += *plin + 2;
	}

	// Footer
	fprintf(pf, DEL_SYMBOL"\n\n");
}

/*
   Print from nLine1 to nLine2 (inclusive).
   If nLine1 == ALL_LINES print all lines.
*/
void PrintFun(FUNCTION *pfun, int nLine1, int nLine2, int fOff)
{
	int n, l, sz, fAll = 0;
	uchar *pLin, *base;

	/* Line 0 (header) */
	pLin = base = (uchar *)pfun + pfun->oSource;
	n = 0;

	/* Special case: print all lines */
	if (nLine1 == ALL_LINES) {
		fAll = 1;
		nLine1 = 1;
		nLine2 = pfun->nLines;
		// Print header (line 0)
		if (fOff)
			print_line("%04d ", pLin - base);
		print_line(g_blanks);
		print_line("%s\n", pLin + 1);
	}

	/* Sanity checks */
	if (nLine1 < 0)
		nLine1 = 0;
	if (nLine2 > pfun->nLines)
		nLine2 = pfun->nLines;

	while (n <= nLine2) {
		sz = *pLin;
		if (nLine1 <= n) {
			if (fOff)
				print_line("%04d ", pLin - base);
			l = print_line("[%d]", n);
			print_line(g_blanks + l);
			print_line("%s\n", pLin + 1);
		}
		pLin += sz + 2;
		++n;
	}

	if (fAll) {
		if (fOff)
			print_line("     ");
		print_line(g_blanks);
		print_line(DEL_SYMBOL"\n");
	}
}

char *FindName(char *pTab, char *pNam, int nLen)
{
	int n;
	char *pch = pTab;

	while ((n = *pch)) {
		if (nLen == n  &&  !strncmp(pNam, pch + 3, n))
			return pch;
		pch += n + 3;
	}

	return NULL;
}

static int CopyNames(char *pdst, char *psrc)
{
	int len, n;

	len = 0;
	while ((n = *psrc)) {
		n += 3;
		if (psrc[1] < FUN_LAB) {
			memcpy(pdst, psrc, n);
			pdst += n;
			len += n;
		}
		psrc += n;
	}

	*pdst = 0;
	return len + 1;
}

static void SwapNames(char *base, char *pfun)
{
	// Before
	//  var1 var2 fun ... varn
	//  ^base     ^pfun

	// After
	// fun var1 var2 ... varn

	char temp[STRINGMAXSIZ+1];
	int flen;
	int vlen;

	flen = *pfun + 3;
	vlen = pfun - base;	// 1 or 2 variables

	// Move function name to temp buffer
	memcpy(temp, pfun, flen);
	// Move var(s) to new position (possible overlapping)
	memmove(base + flen, base, vlen);
	// Move function to beginning of table
	memcpy(base, temp, flen);
	// Zero function name index (cosmetic)
	base[2] = 0;
}

static void AddLabel(LEXER *plex, FUNCTION *pfun, int line)
{
	int n, len;
	char *pch = pfun->aNames;
	char *lab;

	// Label to be added
	len = plex->tokLen;
	lab = plex->ptokBase;

	// Make sure the name is not already in the table
	while ((n = *pch)) {
		if (len == n  &&  !strncmp(lab, pch + 3, n))
			// This lable is already defined
			LexError(plex, LE_BAD_LABEL);
		pch += n + 3;
	}

	// The label was not in the table, so let's add it
	*pch++ = len;
	*pch++ = FUN_LAB;
	*pch++ = line;
	memcpy(pch, lab, len);
	pch += len;

	// Mark end of name table
	*pch++ = '\0';
	pfun->nHdrSiz = OFFSET(pfun,pch);
}

/*
 * Possible header forms:
 * 
 * (1) ∇ fun            (niladic, without return value)
 * (2) ∇ fun y          (monadic, without return value)
 * (3) ∇ x fun y        (dyadic,  without return value)
 * (4) ∇ ret ← fun      (niladic, with return value)
 * (5) ∇ ret ← fun y    (monadic, with return value)
 * (6) ∇ ret ← x fun y  (dyadic,  with return value)
 * 
 */
void ParseHeaderFun(FUNCTION *pfun, LEXER *plex)
{
	char *pNames[4];
	char *pchN, *pc;
	int nNames, nInd, len;

	// Header must start with ∇
	if (plex->tokTyp != APL_DEL)
		LexError(plex, LE_BAD_FUNCTION_HEADER);

	pchN = pfun->aNames;

	NextTok(plex);

	// Must be followed by a name (function or variable)
	if (plex->tokTyp != APL_VARNAM)
		LexError(plex, LE_BAD_FUNCTION_HEADER);

	nNames = 1;
	nInd = 0;
	*pchN++ = plex->tokLen;
	pNames[0] = pchN;
	*pchN++ = FUN_NAM;
	*pchN++ = nInd++;
	memcpy(pchN, plex->ptokBase, plex->tokLen);
	pchN += plex->tokLen;
	*pchN = '\0';

	NextTok(plex);

	if (plex->tokTyp == APL_LEFT_ARROW) {
		// ∇ ret ← ...
		pfun->nRet = 1;
		NextTok(plex);
	}

	while (plex->tokTyp == APL_VARNAM) {
		/* ret ← x fun y: Max = 4 names */
		if (nNames == 4)
			LexError(plex, LE_BAD_FUNCTION_HEADER);
		if (FindName(pfun->aNames, plex->ptokBase, plex->tokLen))
			LexError(plex, LE_BAD_FUNCTION_HEADER);
		*pchN++ = plex->tokLen;
		pNames[nNames++] = pchN;
		*pchN++ = FUN_NAM;
		*pchN++ = nInd++;
		memcpy(pchN, plex->ptokBase, plex->tokLen);
		pchN += plex->tokLen;
		*pchN = '\0';
		NextTok(plex);
	}

	/*
	** The function's name table looks like this:
	**
	** R <- A fun B; C; D...
	**
	** +------+---+---+---+---+---+ ... +-+
	** | fun  | R | A | B | C | D |     |0|
	** +------+---+---+---+---+---+ ... +-+
	**
	** Each name is preceded by three bytes:
	**
	** +---+---+---+-------------+
	** |LEN|TYP|IND| Name        |
	** +---+---+---+-------------+
	**              \___ LEN ___/
	**
	**   where: LEN is the name length
	**          TYP is the name type
	**          IND is the name index in the stack
	**          Name is not 0-terminated.
	**
	** Valid types are:
	**
	**     FUN_NAM = 0 -> Function name (fun)
	**     FUN_RET = 1 -> Return value (R)
	**     FUN_ARG = 2 -> Function argument (A, B)
	**     FUN_LOC = 3 -> Local variables (C, D, ...)
	**     FUN_LAB = 4 -> Function labels (added later)
	**     FUN_GLB = 5 -> Global variables (added later)
	*/

	if (pfun->nRet) {		// There is a return value
		switch (nNames) {
		case 1:		// Invalid
			LexError(plex, LE_BAD_FUNCTION_HEADER);
			break;
		case 2:		// R <- fun
			pc = pNames[0];
			*pc = FUN_RET;
			nInd = 1;
			SwapNames(pNames[0]-1, pNames[1]-1);
			break;
		case 3:		// R <- fun A
			pc = pNames[0];		// Patch R
			*pc = FUN_RET;
			pc = pNames[2];		// Patch A
			*pc++ = FUN_ARG;
			*pc = 1;
			pfun->nArgs = 1;
			nInd = 2;
			SwapNames(pNames[0]-1, pNames[1]-1);
			break;
		case 4:		// R <- A fun B
			pc = pNames[0];		// Patch R
			*pc = FUN_RET;
			pc = pNames[1];		// Patch A
			*pc++ = FUN_ARG;
			*pc = 1;
			pc = pNames[3];		// Patch B
			*pc++ = FUN_ARG;
			*pc = 2;
			pfun->nArgs = 2;
			nInd = 3;
			SwapNames(pNames[0]-1, pNames[2]-1);
			break;
		}
	} else {				// No return value
		switch (nNames) {
		case 1:		// fun
			nInd = 0;
			break;
		case 2:		// fun A
			pc = pNames[1];		// Patch A
			*pc++ = FUN_ARG;
			*pc = 0;
			pfun->nArgs = 1;
			nInd = 1;
			break;
		case 3:		// A fun B
			pc = pNames[0];		// Patch A
			*pc++ = FUN_ARG;
			pc = pNames[2];		// Patch B
			*pc++ = FUN_ARG;
			*pc = 1;
			pfun->nArgs = 2;
			nInd = 2;
			SwapNames(pNames[0]-1, pNames[1]-1);
			break;
		case 4:		// Invalid
			LexError(plex, LE_BAD_FUNCTION_HEADER);
			break;
		}
	}

	// Parse list of local variable names
	while (plex->tokTyp == ';') {
		NextTok(plex);
		if (plex->tokTyp != APL_VARNAM)
			LexError(plex, LE_BAD_FUNCTION_HEADER);
		if (FindName(pfun->aNames, plex->ptokBase, plex->tokLen))
			LexError(plex, LE_BAD_FUNCTION_HEADER);
		*pchN++ = plex->tokLen;
		*pchN++ = FUN_LOC;
		*pchN++ = nInd++;
		memcpy(pchN, plex->ptokBase, plex->tokLen);
		pchN += plex->tokLen;
		++pfun->nLocals;
		NextTok(plex);
	}

	// Mark end of name table
	*pchN++ = '\0';
	pfun->nHdrSiz = OFFSET(pfun,pchN);

	// Re-index the variables in reverse order
	// That's how they are accessed during function evaluation
	pchN = pfun->aNames;
	while ((len = *pchN)) {
		if (pchN[1] != FUN_NAM)
			pchN[2] = --nInd;
		pchN += len + 3;
	}

	if (plex->tokTyp != APL_END)
		LexError(plex, LE_BAD_FUNCTION_HEADER);
}
