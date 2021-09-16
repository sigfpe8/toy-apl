// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "apl.h"
#include "error.h"
#include "token.h"

static void		ArrayInfo(ARRAYINFO *pai);
static double	Binomial(double x, double y);
static int		Conformable(DESC *pv1, DESC *pv2);
static void		EvlAtom(ENV *penv);
static int		EvlBranchLine(int old);
static double	EvlCircularFun(int fun, double arg);
static void		EvlDyadicFun(int fun, int axis, int axis_type);
static void		EvlDyadicNumFun(int fun);
static void		EvlDyadicStrFun(int fun);
static void		EvlFunction(FUNCTION *pfun);
static void		EvlGetIndex(int n);
static int		EvlIndex(ENV *penv);
static void		EvlInnerProd(int funL, int funR);
static void		EvlSetIndex(int n);
static void		EvlMonadicFun(ENV *penv, int fun, int axis, int axis_type);
static void		EvlOuterProd(int fun);
static void		ExtendArray(ARRAYINFO *pai, int axis);
static void		ExtendScalar(ARRAYINFO *psrc, ARRAYINFO *pdst, int axis);
static void		FunCatenate(int axis, int axis_type);
static void		FunCompress(int axis);
static void		FunDeal(void);
static void		FunDecode(void);
static void		FunDrop(void);
static void		FunEncode(void);
static void		FunExecute(ENV *penv);
static void		FunExpand(int axis);
static void		FunFormat(void);
static void		FunFormat2(void);
static void		FunGradeUpDown(int fun);
static void		FunIota(void);
static void		FunIndexOf(void);
static void		FunMatDivide(void);
static void		FunMatInverse(void);
static void		FunMembership(void);
static void		FunReshape(void);
static void		FunReverse(int axis);
static void		FunRotate(int axis);
static void		FunShape(void);
static void		FunSystem1(int fun);
static void		FunTake(void);
static void		FunTranspose(void);
static int		IsNullArray(DESC *pd);
static int		NumElem(DESC *pv);
static void		OperPush(int type, int rank);
static void		OperPushDesc(DESC *pd);
static void		OperSwap(void);
static void		QuadInp(ENV *penv);
static void		QuoteQuadInp(void);
static void		Reduce(int fun, int dim);
static void		Scan(int fun, int dim);
static void		SysIdent(void);
static void		SysRref(void);
static FUNCTION* VarGetFun(ENV *penv);
static void		VarGetNam(ENV *penv);
static void		VarGetInx(ENV *penv);
static void		VarGetSys(ENV *penv);
static void		VarSetInx(ENV *penv, int dims);
static void		VarSetNam(ENV *penv, int dims);
static void		VarSetSys(ENV *penv);

char *apchEvlMsg[] =
{
	"No error",					/*  0 */
	"Not an atom",				/*  1 */
	"Bad function",				/*  2 */
	"Unmatched parentheses",	/*  3 */
	"Domain error",				/*  4 */
	"Not conformable",			/*  5 */
	"Operand stack overflow",	/*  6 */
	"Array stack overflow",		/*  7 */
	"Divide by zero",			/*  8 */
	"Name table full",			/*  9 */
	"Undefined variable",		/* 10 */
	"Global desc table full",	/* 11 */
	"Heap full",				/* 12 */
	"Unmatched brackets",		/* 13 */
	"Invalid index",			/* 14 */
	"No return value",			/* 15 */
	"Syntax error",				/* 16 */
	"Rank error",				/* 17 */
	"Length error",				/* 18 */
	"Not implemented",			/* 19 */
	"Invalid axis",				/* 20 */
	"Read-only system variable",/* 21 */
	"No value"					/* 22 */
};

void InitEnvFromLexer(ENV *penv, LEXER *plex)
{
	penv->pFunction = 0;
	penv->pCode = plex->pCode + 1;
	penv->plitBase = plex->plitBase;
	penv->plinBase = plex->plinBase;
	penv->pvarBase = poprBase + 1;	// Used by NUM_VALS()
	penv->flags = 0;
}

void InitEnvFromFunction(ENV *penv, FUNCTION *pfun)
{
	penv->pFunction = pfun;
	penv->pCode = POINTER(pfun, pfun->oObject);
	penv->plitBase = POINTER(pfun, pfun->nHdrSiz);
	penv->plinBase = (offset *)(penv->plitBase + pfun->nLits);
	penv->pvarBase = 0;		// Set in EvlFunction()
	penv->flags = 0;
}

// Called to evaluate a line from:
//   1. a REPL input
//   2. a function being executed
//   3. an execute (⍎) string
//   4. a ⎕ input string (via execute)
//   5. a file being loaded
void EvlExprList(ENV *penv)
{
	// Evaluate list of diamond-separated expressions
	// expr1 ⋄ expr2 ⋄ ... ⋄ exprn
	do {
		EvlExpr(penv);
		// Print and drop the value of all expressions.
		// Keep the last one if KEEP_LAST flag is set.
		if ((NUM_VALS(penv) > 0) && *penv->pCode != APL_RIGHT_ARROW &&
			(*penv->pCode == APL_DIAMOND || !KEEP_LAST(penv))) {
			DescPrint(poprTop);		// Print top value
			print_line("\n");
			POP(poprTop);			// Drop it
		}
	} while (*penv->pCode++ == APL_DIAMOND);

	--penv->pCode;	// Point back to token that ended the expression
}

// Reset evaluation stacks
void EvlResetStacks(void)
{
	poprTop = poprBase + 1;
	parrTop = parrBase;
}

void EvlExpr(ENV *penv)
{
	int fun, nxt;
	int axis;		// Axis number (0-based)
	int axis_type;	// Axis type
	int dims = 0;	// # of dimensions in indexed assignment

	if (*penv->pCode == APL_END || *penv->pCode == APL_NL || *penv->pCode == APL_DIAMOND)
		return;

	if (IsAtom(*penv->pCode))
		EvlAtom(penv);
	else
		EvlError(EE_NOT_ATOM);

	while (!IsEnd(*penv->pCode)) {
		// Axis specification?
		if (*penv->pCode == APL_RIGHT_BRACKET) {
			++penv->pCode;
			EvlExpr(penv);
			if (*penv->pCode++ != APL_LEFT_BRACKET)
				EvlError(EE_UNMATCHED_BRACKETS);

			if (!ISNUMBER(poprTop) || !ISSCALAR(poprTop))
				EvlError(EE_DOMAIN);

			axis = (int)VNUM(poprTop);
			if ((double)axis == VNUM(poprTop)) {
				// Integer: regular axis
				axis_type = AXIS_REGULAR;
			} else {
				// Non-integer: lamination
				axis = (int)ceil(VNUM(poprTop));
				axis_type = AXIS_LAMINATE;
			}
			if (axis < g_origin)
				EvlError(EE_INVALID_AXIS);
			axis -= g_origin;
			POP(poprTop);
		} else {
			// If axis was not spcecified use the default
			axis = -1;
			axis_type = AXIS_DEFAULT;
		}

		fun = *penv->pCode;
		nxt = *(penv->pCode + 1);
		if (IsAssign(fun)) {
			++penv->pCode;	// Skip ←

			// Must have a value
			VALIDATE_ARGS(penv,1);

			// Indexed assignment?
			if (*penv->pCode == APL_RIGHT_BRACKET)
				dims = EvlIndex(penv);

			switch (*penv->pCode) {
			case APL_VARINX:
				VarSetInx(penv, dims);
				break;
			case APL_VARNAM:
				VarSetNam(penv, dims);
				break;
			case APL_VARSYS:
				if (dims)
					EvlError(EE_SYNTAX_ERROR);
				VarSetSys(penv);
				break;
			case APL_QUAD:
				if (dims)
					EvlError(EE_SYNTAX_ERROR);
				DescPrintln(poprTop);
				++penv->pCode;
				break;
			case APL_QUOTE_QUAD:
				if (dims)
					EvlError(EE_SYNTAX_ERROR);
				DescPrint(poprTop);
				++penv->pCode;
				break;
			default:
				EvlError(EE_BAD_FUNCTION);
			}
			dims = 0;
			if (*penv->pCode == APL_DIAMOND)
				POP(poprTop);
			else if ((*penv->pCode == APL_END || *penv->pCode == APL_NL) && !KEEP_LAST(penv))
				POP(poprTop);
		} else if ((fun == APL_SLASH || fun == APL_SLASH_BAR) && IsDyadic(nxt)) {
			VALIDATE_AXIS(poprTop,APL_SLASH);
			Reduce(nxt, axis);
			penv->pCode += 2;
		} else if ((fun == APL_BACKSLASH || fun == APL_BACKSLASH_BAR) && IsDyadic(nxt)) {
			VALIDATE_AXIS(poprTop,APL_BACKSLASH);
			Scan(nxt, axis);
			penv->pCode += 2;
		} else if (IsDyadic(fun) && IsAtom(nxt)) {
			++penv->pCode;
			EvlAtom(penv);
			VALIDATE_ARGS(penv,2);
			EvlDyadicFun(fun, axis, axis_type);
		} else if (IsDyadic(fun) && nxt == APL_DOT && IsAtom(*(penv->pCode + 3))) {
			if (*(penv->pCode + 2) == APL_JOT ) {		// Outer product: A ∘. fun B
				penv->pCode += 3;
				EvlAtom(penv);
				VALIDATE_ARGS(penv,2);
				EvlOuterProd(fun);
			} else if (IsDyadic(*(penv->pCode + 2))) {	// Inner product:  A fun2 . fun B
				int fun2 = *(penv->pCode + 2);
				penv->pCode += 3;
				EvlAtom(penv);
				VALIDATE_ARGS(penv,2);
				EvlInnerProd(fun2, fun);
			} else
				EvlError(EE_SYNTAX_ERROR);
		} else if (IsMonadic(fun)) {
			VALIDATE_ARGS(penv,1);
			++penv->pCode;
			if (fun == APL_SYSFUN1) {		// ⎕fun
				fun = *penv->pCode++;
				FunSystem1(fun);
			} else
				EvlMonadicFun(penv, fun, axis, axis_type);
		} else if (fun == APL_VARNAM) {
			// User-defined function?
			++penv->pCode;
			FUNCTION *pfun = VarGetFun(penv);
			if (pfun->nArgs == 2 && IsAtom(*penv->pCode)) {
				// Dyadic user function
				EvlAtom(penv);
				VALIDATE_ARGS(penv,2);
			} else if (pfun->nArgs == 1) {
				VALIDATE_ARGS(penv,1);
			} else
				EvlError(EE_BAD_FUNCTION);
			EvlFunction(pfun);
		} else
			EvlError(EE_SYNTAX_ERROR);
	}
}

static void EvlAtom(ENV *penv)
{
	int len;
	int dims = 0;

	// An index applies to any atom (not just a variable)
	if (*penv->pCode == APL_RIGHT_BRACKET)
		dims = EvlIndex(penv);

	switch (*penv->pCode++) {
	case APL_NUM:
		OperPush(TNUM,0);
		VNUM(poprTop) = penv->plitBase[*penv->pCode++];
		break;

	case APL_CHR:
		OperPush(TCHR,0);
		VCHR(poprTop) = *penv->pCode++;
		break;

	case APL_ARR:
		OperPush(TNUM,1);
		SHAPE(poprTop)[0] = *penv->pCode++;
		VOFF(poprTop) = WKSOFF(penv->plitBase + *penv->pCode++);
		break;

	case APL_STR:
		OperPush(TCHR,1);
		len = *penv->pCode++;
		SHAPE(poprTop)[0] = len;
		VOFF(poprTop) = WKSOFF(penv->pCode);
		penv->pCode += len;
		break;

	case APL_VARNAM:
		VarGetNam(penv);
		break;

	case APL_VARINX:
		VarGetInx(penv);
		break;

	case APL_VARSYS:
		VarGetSys(penv);
		break;

	case APL_RIGHT_PAREN:
		EvlExpr(penv);
		if (*penv->pCode != APL_LEFT_PAREN)
			EvlError(EE_UNMATCHED_PAR);
		++penv->pCode;
		break;

	case APL_QUAD:
		QuadInp(penv);
		break;

	case APL_QUOTE_QUAD:
		QuoteQuadInp();
		break;
	}

	if (dims)
		EvlGetIndex(dims);
}

static int EvlIndex(ENV *penv)
{
	int dims = 0;

	do {
		++penv->pCode;	// Skip ] or ;
		// Empty slot?
		// We push TUND to mean "use the full axis"
		// Example: A[;5] = all elements of column 5
		if (*penv->pCode == APL_SEMICOLON || *penv->pCode == APL_LEFT_BRACKET)
			OperPush(TUND,0);
		else
			EvlExpr(penv);
		++dims;
	} while (*penv->pCode == APL_SEMICOLON);
	if (*penv->pCode != APL_LEFT_BRACKET)
		EvlError(EE_UNMATCHED_BRACKETS);

	++penv->pCode;
	return dims;
}

static void EvlGetIndex(int n)
{
	INDEX	indices[MAXDIM];
	aplshape	shape[MAXDIM];		// Shape of result array
	int		d, i, j, ind, len, m, r, t;
	DESC	*popr;
	void	*parr;
	INDEX	*p;

	// X[I]

	// Stack has: top -> X
	//                   I (n)
	if (!ISARRAY(poprTop))
		EvlError(EE_DOMAIN);
	// Rank of X must be equal to the number of indices (n)
	if (RANK(poprTop) != n)
		EvlError(EE_NOT_CONFORMABLE);

	// Calculate rank and shape of result
	// Shape of result is , of the shapes of the indices
	// d -> dimension 0 <= d < n = RANK(array)
	// r -> rank of result array (sum of ranks of indices)
	r = 0;
	for (d = 0, popr = poprTop + 1; d < n; ++d, ++popr) {
		switch (TYPE(popr)) {
		case TUND:				// All indices
			if (r + 1 > MAXDIM)
				EvlError(EE_ARRAY_OVERFLOW);
			shape[r++] = SHAPE(poprTop)[d];
			break;
		case TNUM:
			if (ISSCALAR(popr))	// Single index
				break;
			i = RANK(popr);		// Array of indices
			if (r + i > MAXDIM)
				EvlError(EE_ARRAY_OVERFLOW);
			memcpy(shape + r, SHAPE(popr), i * sizeof(aplshape));
			r += i;
			break;
		default:
			EvlError(EE_INVALID_INDEX);
		}
	}

	// Create index iterator and get first index
	ind = CreateIndex(indices, n);

	parr = VPTR(poprTop);
	t = TYPE(poprTop);

	// Result is a single element (rank = 0)
	if (!r) {
		if (t == TNUM) {
			double *pold;
			// 1 real number
			pold  = parr;
			poprTop += n;
			TYPE(poprTop) = TNUM;
			RANK(poprTop) = 0;
			VNUM(poprTop) = pold[ind];
			return;
		} else {	// TCHR
			char *pold;
			// 1 character
			pold  = parr;
			poprTop += n;
			TYPE(poprTop) = TCHR;
			RANK(poprTop) = 0;
			VCHR(poprTop) = pold[ind];
			return;
		}
	}

	// Result is an array

	// Calculate number of elements
	m = 1;
	for (i = 0; i < r; ++i)
		m *= shape[i];

	// Drop indices and create new descriptor for result
	poprTop += n;
	TYPE(poprTop) = t;
	RANK(poprTop) = r;
	memcpy(SHAPE(poprTop), shape, r * sizeof(aplshape));

	// Copy indexed elements to new array
	if (t == TNUM) {
		double *pold, *pnew;

		pold  = parr;
		pnew = TempAlloc(sizeof(double), m);
		VOFF(poprTop) = WKSOFF(pnew);
		do {
			*pnew++ = pold[ind];
			ind = NextIndex(indices, n);
		} while (ind >= 0);
	} else {	// TCHR
		char *pold, *pnew;

		pold  = parr;
		pnew = TempAlloc(sizeof(char), m);
		VOFF(poprTop) = WKSOFF(pnew);
		do {
			*pnew++ = pold[ind];
			ind = NextIndex(indices, n);
		} while (ind >= 0);
	}
}

static void EvlSetIndex(int n)
{
	INDEX	indices[MAXDIM];
	aplshape	shape[MAXDIM];		// Shape of index array
	int		i, d, ind, r, t;
	DESC	*popr;
	int		step;
	void	*parr, *pval;

	// X[I]←Y

	// Stack has: top -> X
	//                   I (n)
	//                   Y
	if (!ISARRAY(poprTop))
		EvlError(EE_DOMAIN);
	// Rank of X must be equal to the number of indices (n)
	if (RANK(poprTop) != n)
		EvlError(EE_NOT_CONFORMABLE);

	// Create index iterator and get first index
	ind = CreateIndex(indices, n);
	parr = VPTR(poprTop);
	t = TYPE(poprTop);

	// Calculate rank and shape of index (I)
	// It must conform to the shape of the value (Y)
	// r -> rank of index (sum of ranks of indices)
	r = 0;
	for (i = 0, popr = poprTop + 1; i < n; ++i, ++popr) {
		switch (TYPE(popr)) {
		case TUND:				// All indices
			if (r + 1 > MAXDIM)
				EvlError(EE_ARRAY_OVERFLOW);
			shape[r++] = SHAPE(poprTop)[i];
			break;
		case TNUM:
			if (ISSCALAR(popr))	// Single index
				break;
			d = RANK(popr);		// Array of indices
			if (r + i > MAXDIM)
				EvlError(EE_ARRAY_OVERFLOW);
			memcpy(shape + r, SHAPE(popr), d * sizeof(aplshape));
			r += d;
			break;
		default:
			EvlError(EE_INVALID_INDEX);
		}
	}

	// Drop old value (X) and indices (I)
	// Leave Y on the top
	poprTop += n + 1;

	if (TYPE(poprTop) != t)
		EvlError(EE_DOMAIN);
	if (ISARRAY(poprTop)) {
		// I and Y must have the same shape
		if (RANK(poprTop) != r)
			EvlError(EE_NOT_CONFORMABLE);
		for (i = 0; i < r; ++i)
			if (SHAPE(poprTop)[i] != shape[i])
				EvlError(EE_NOT_CONFORMABLE);
		step = 1;
		pval = VPTR(poprTop);	
	} else {
		step = 0;
		pval = (void *)&poprTop->uval;
	}

	// Copy indexed elements to array
	if (t == TNUM) {
		double *psrc, *pdst;

		pdst = parr;
		psrc = pval;
		do {
			pdst[ind] = *psrc;
			psrc += step;
			ind = NextIndex(indices, n);
		} while (ind >= 0);
	} else {	// TCHR
		char *psrc, *pdst;

		pdst = parr;
		psrc = pval;
		do {
			pdst[ind] = *psrc;
			psrc += step;
			ind = NextIndex(indices, n);
		} while (ind >= 0);
	}

	// Leave Y on the stack
}

// Create index iterator and return 1st index
//
//   The stack contains:
//   poprTop --> target array
//        +1 --> 1st index (leftmost)
//        ...
//        +n --> last index (rightmost)
int CreateIndex(INDEX *pi, int n)
{
	INDEX	*pe = pi + n;
	INDEX	*p;
	DESC	*popr;
	int		d;
	int		ind;
	int		size;

	ind = 0;

	// First scan the array backwards to fill in
	// the shape and size of each dimension
	size = 1; // Size of last dimension (columns)
	p = pe;
	for (d = n - 1; d >= 0; --d) {
		--p;
		p->shape = SHAPE(poprTop)[d];
		p->size = size;
		size *= p->shape;
	}

	// Now scan the index specifiers and fill in the rest
	for (p = pi, popr = poprTop + 1; p < pe; ++p, ++popr) {
		// Each index must be either a number or a numeric array
		switch (TYPE(popr)) {
		case TUND:					// All indices       M[2;]
			p->index = 0;
			p->type = TUND;
			break;
		case TNUM:
			if (ISSCALAR(popr)) {	// Single index      M[2;3]
				p->index = (int)VNUM(popr) - g_origin;
				p->type = TINT;
				break;
			} else {				// Array of indices  M[2 3;4 5]		
				p->beg = p->ptr = VPTR(popr);
				p->end = p->beg + NumElem(popr);
				p->index = *p->ptr - g_origin;
				p->type = TNUM;
			}
			break;
		default:
			EvlError(EE_INVALID_INDEX);
		}

		if (p->index < 0 || p->index >= p->shape)
			EvlError(EE_INVALID_INDEX);
		ind += p->index * p->size;
	}

	return ind;
}

int NextIndex(INDEX *pi, int n)
{
	INDEX *pe;
	INDEX *p;
	int ind;

	// Starting from the last axis, try to advance
	// the index. If last position was already used,
	// set it back to the first position and backtrack
	// to the previous axis.
	p = pe = pi + n;
	while (p > pi) {
		--p;
		switch (p->type) {
		case TUND:
			if (++p->index < p->shape) goto get_index;
			p->index = 0;
			break;
		case TINT:
			// Cannot advance; must backtrack
			break;
		case TNUM:
			if (++p->ptr < p->end) {
				p->index = *p->ptr - g_origin;
				if (p->index < 0 || p->index >= p->shape)
					EvlError(EE_INVALID_INDEX);
				goto get_index;
			}
			p->ptr = p->beg;
			p->index = *p->ptr - g_origin;
			break;
		}
	}
		
	// All indices have been generated
	return -1;

get_index:
	ind = 0;
	for (p = pi; p < pe; ++p)
		ind += p->size * p->index;

	return ind;
}

int CreateTakeIndex(TAKEINDEX *pi, int *dst_shape, int *src_shape, int rank, int *pdst_ind, int *psrc_ind)
{
	TAKEINDEX	*p;
	int		d;
	int		n;
	int		src_ind, dst_ind;
	int		src_size, dst_size;

	// First scan the source and destination arrays backwards to
	// fill in the shapes and sizes of each dimension
	src_size = 1; // Size of last dimension (columns)
	dst_size = 1; // Size of last dimension (columns)
	p = pi + rank;
	for (d = rank - 1; d >= 0; --d) {
		--p;
		p->src.shape = src_shape[d];		// Always >= 0
		p->src.size = src_size;
		src_size *= p->src.shape;

		p->dst.shape = abs(dst_shape[d]);	// Can be negative
		p->dst.size = dst_size;
		dst_size *= p->dst.shape;
	}

	src_ind = dst_ind = 0;
	p = pi;
	for (d = 0; d < rank; ++d) {
		n = dst_shape[d];
		if (n > 0) {	// From the beginning
			p->src.first = p->src.index = 0;
			p->dst.first = p->dst.index = 0;
			p->src.last = (n > p->src.shape) ? p->src.shape - 1 : n - 1;
			p->dst.last = p->dst.shape - 1;
		} else if (n < 0) {	// From the end
			n = -n;
			if (n > p->src.shape) {
				p->src.first = p->src.index = 0;
				p->dst.first = p->dst.index = n - p->src.shape;
			} else {
				p->src.first = p->src.index = p->src.shape - n;
				p->dst.first = p->dst.index = 0;
			}
			p->src.last = p->src.shape - 1;
			p->dst.last = p->dst.shape - 1;
		} else {	// n = 0
			p->src.first = p->src.last = p->src.index = -1;
			p->dst.first = p->dst.last = p->dst.index = -1;
		}
		src_ind += p->src.index * p->src.size;
		dst_ind += p->dst.index * p->dst.size;
		++p;
	}

	*psrc_ind = src_ind;
	*pdst_ind = dst_ind;

	// Return length of row to copy
	p = pi + rank - 1;
	return p->src.last - p->src.first + 1;
}

int NextTakeIndex(TAKEINDEX *pi, int rank, int *pdst_ind, int *psrc_ind)
{
	TAKEINDEX *pe;
	TAKEINDEX *p;
	int src_ind, dst_ind;

	// We have copied a row (last axis) so there's no need
	// to check it. Starting from the last but one axis,
	// try to advance the index, backtraking to the previous
	// level when necessary.

	pe = pi + rank;
	p = pe - 1;
	while (p > pi) {
		--p;
		if (++p->src.index <= p->src.last)
			goto get_index;

		// Reset this level and backtrack
		p->src.index = p->src.first;
		p->dst.index = p->dst.first;
	}
		
	// All indices have been generated
	return 0;	// Stop

get_index:
	++p->dst.index;
	src_ind = dst_ind = 0;
	for (p = pi; p < pe; ++p) {
		src_ind += p->src.index * p->src.size;
		dst_ind += p->dst.index * p->dst.size;
	}

	*psrc_ind = src_ind;
	*pdst_ind = dst_ind;

	return 1;	// Continue
}

int CreateDropIndex(DROPINDEX *pi, int *dst_drops, int *src_shape, int rank, int *psrc_ind)
{
	DROPINDEX	*p;
	int		d;
	int		n;
	int		src_ind;
	int		src_size;

	// First scan the source array backwards to
	// fill in the shapes and sizes of each dimension
	src_size = 1; // Size of last dimension (columns)
	p = pi + rank;
	for (d = rank - 1; d >= 0; --d) {
		--p;
		p->src.shape = src_shape[d];		// Always >= 0
		p->src.size = src_size;
		src_size *= p->src.shape;
	}

	src_ind = 0;
	p = pi;
	for (d = 0; d < rank; ++d) {
		n = dst_drops[d];
		if (n > 0) {	// From the beginning
			p->src.first = p->src.index = n;
			p->src.last = p->src.shape - 1;
		} else if (n < 0) {	// From the end
			p->src.first = p->src.index = 0;
			p->src.last = p->src.shape + n - 1;
		} else {	// n = 0, don't drop anything
			p->src.first = p->src.index = 0;
			p->src.last = p->src.shape - 1;
		}
		src_ind += p->src.index * p->src.size;
		++p;
	}

	*psrc_ind = src_ind;

	// Return length of row to copy
	p = pi + rank - 1;
	return p->src.last - p->src.first + 1;
}

int NextDropIndex(DROPINDEX *pi, int rank, int *psrc_ind)
{
	DROPINDEX *pe;
	DROPINDEX *p;
	int src_ind;

	// We have copied a row (last axis) so there's no need
	// to check it. Starting from the last but one axis,
	// try to advance the index, backtraking to the previous
	// level when necessary.

	pe = pi + rank;
	p = pe - 1;
	while (p > pi) {
		--p;
		if (++p->src.index <= p->src.last)
			goto get_index;

		// Reset this level and backtrack
		p->src.index = p->src.first;
	}
		
	// All indices have been generated
	return 0;	// Stop

get_index:
	src_ind = 0;
	for (p = pi; p < pe; ++p)
		src_ind += p->src.index * p->src.size;

	*psrc_ind = src_ind;

	return 1;	// Continue
}

static inline double EvlDyadicScalarNumFun(int fun, double numL, double numR)
{
	double res;

	switch (fun) {
	case APL_CIRCLE:
		res = EvlCircularFun((int)numL, numR);
		break;
	case APL_UP_STILE:
		res = max(numL, numR);
		break;
	case APL_DOWN_STILE:
		res = min(numL, numR);
		break;
	case APL_PLUS:
		res = numL + numR;
		break;
	case APL_MINUS:
		res = numL - numR;
		break;
	case APL_LESS_THAN:
		res = numL < numR;
		break;
	case APL_EQUAL:
		res = numL == numR;
		break;
	case APL_GREATER_THAN:
		res = numL > numR;
		break;
	case APL_TIMES:
		res = numL * numR;
		break;
	case APL_DIV:
		if (!numR)
			EvlError(EE_DIVIDE_BY_ZERO);
		res = numL / numR;
		break;
	case APL_EXCL_MARK:
		res = Binomial(numL, numR);
		break;
	case APL_STILE:
		if (numL != 0)
			res = fmod(numR, numL);
		else {
			if (numR >= 0)
				res = numR;
			else
				EvlError(EE_DOMAIN);
		}
		break;
	case APL_STAR:
		res = pow(numL, numR);
		break;
	case APL_AND:
		if ((numL != 0 && numL != 1) || (numR != 0 && numR != 1))
			EvlError(EE_DOMAIN);
		res = numL && numR;
		break;
	case APL_OR:
		if ((numL != 0 && numL != 1) || (numR != 0 && numR != 1))
			EvlError(EE_DOMAIN);
		res = numL || numR;
		break;
	case APL_NAND:
		if ((numL != 0 && numL != 1) || (numR != 0 && numR != 1))
			EvlError(EE_DOMAIN);
		res = !(numL && numR);
		break;
	case APL_NOR:
		if ((numL != 0 && numL != 1) || (numR != 0 && numR != 1))
			EvlError(EE_DOMAIN);
		res = !(numL || numR);
		break;
	case APL_LT_OR_EQUAL:
		res = numL <= numR;
		break;
	case APL_NOT_EQUAL:
		res = numL != numR;
		break;
	case APL_GT_OR_EQUAL:
		res = numL >= numR;
		break;
	}

	return res;
}

static double EvlCircularFun(int fun, double arg)
{
	switch (fun) {
	case -7:	// Inverse hyperbolic tangent (1>|arg)
		if (arg > -1.0 && arg < 1.0)
			return atanh(arg);
		break;
	case -6:	// Inverse hyperbolic cosine (arg≥1)
		if (arg >= 1.0)
			return acosh(arg);
		break;
	case -5:	// Inverse hyperbolic sine
		return asinh(arg);
	case -4:	// Sqrt(-1 + arg^2) (1≤|arg)
		if (arg <= -1.0 || arg >= 1.0)
			return sqrt(-1.0 + arg * arg);
		break;
	case -3:	// Arc tangent
		return atan(arg);
	case -2:	// Arc cosine (1≥|arg)
		if (arg >= -1.0 && arg <= 1.0)
			return acos(arg);
		break;
	case -1:	// Arc sine (1≥|arg)
		if (arg >= -1.0 && arg <= 1.0)
			return asin(arg);
		break;
	case 0:		// Sqrt(1 - arg^2) ((|arg)≤1)
		if (arg >= -1.0 && arg <= 1.0)
			return sqrt(1.0 - arg * arg);
		break;
	case 1:		// Sine
		return sin(arg);
	case 2:		// Cosine
		return cos(arg);
	case 3:		// Tangent
		return tan(arg);
	case 4:		// Sqrt(1 + arg^2)
		return sqrt(1.0 + arg * arg);
	case 5:		// Hyperbolic sine
		return sinh(arg);
	case 6:		// Hyperbolic cosine
		return cosh(arg);
	case 7:		// Hyperbolic tanget
		return tanh(arg);
	}

	EvlError(EE_DOMAIN);
	return 0;	// Not reached
}

static void EvlDyadicStrFun(int fun)
{
	char *pchrL, *pchrR;
	double *pnew;
	int stepL, stepR;
	int nelem;
	int rank;

	POP(poprTop);

	switch (CMP_TYPES(poprTop-1, poprTop)) {
	case CMP_SCALAR_SCALAR:
		pchrL = &(poprTop-1)->uval.xchr[0];
		stepL = 0;
		pchrR = &(poprTop)->uval.xchr[0];
		stepR = 0;
		nelem = 1;
		break;
	case CMP_SCALAR_ARRAY:
		pchrL = &(poprTop-1)->uval.xchr[0];
		stepL = 0;
		pchrR = VPTR(poprTop);
		stepR = 1;
		nelem = NumElem(poprTop);
		break;
	case CMP_ARRAY_SCALAR:
		pchrL = VPTR(poprTop-1);
		stepL = 1;
		pchrR = &(poprTop)->uval.xchr[0];
		stepR = 0;
		rank = RANK(poprTop-1);
		nelem = NumElem(poprTop-1);
		// Copy shape and rank to result
		RANK(poprTop) = rank;
		for (int i = 0; i < rank; ++i)
			SHAPE(poprTop)[i] = SHAPE(poprTop-1)[i];
		break;
	case CMP_ARRAY_ARRAY:
		if (!Conformable(poprTop-1,poprTop))
			EvlError(EE_NOT_CONFORMABLE);
		pchrL = VPTR(poprTop-1);
		stepL = 1;
		pchrR = VPTR(poprTop);
		stepR = 1;
		nelem = NumElem(poprTop);
		break;
	}

	if (nelem == 1) {	// Result is a scalar
		pnew = &poprTop->uval.xdbl;
	} else {			// Result is an array
		pnew = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pnew);
	}
	TYPE(poprTop) = TNUM;

	switch (fun) {
	case APL_EQUAL:
		while (nelem--) {
			*pnew++ = *pchrL == *pchrR;
			pchrL += stepL;
			pchrR += stepR;
		}
		break;
	case APL_NOT_EQUAL:
		while (nelem--) {
			*pnew++ = *pchrL != *pchrR;
			pchrL += stepL;
			pchrR += stepR;
		}
		break;
	}
}

static void EvlDyadicNumFun(int fun)
{
	double *pnumL, *pnumR;
	double *pnew;
	int stepL, stepR;
	int nelem;
	int rank;

	POP(poprTop);

	switch (CMP_TYPES(poprTop-1, poprTop)) {
	case CMP_SCALAR_SCALAR:
		pnumL = &(poprTop-1)->uval.xdbl;
		stepL = 0;
		pnumR = &(poprTop)->uval.xdbl;
		stepR = 0;
		nelem = 1;
		break;
	case CMP_SCALAR_ARRAY:
		pnumL = &(poprTop-1)->uval.xdbl;
		stepL = 0;
		pnumR = VPTR(poprTop);
		stepR = 1;
		nelem = NumElem(poprTop);
		break;
	case CMP_ARRAY_SCALAR:
		pnumL = VPTR(poprTop-1);
		stepL = 1;
		pnumR = &(poprTop)->uval.xdbl;
		stepR = 0;
		rank = RANK(poprTop-1);
		nelem = NumElem(poprTop-1);
		// Copy shape and rank to result
		RANK(poprTop) = rank;
		for (int i = 0; i < rank; ++i)
			SHAPE(poprTop)[i] = SHAPE(poprTop-1)[i];
		break;
	case CMP_ARRAY_ARRAY:
		if (!Conformable(poprTop-1,poprTop))
			EvlError(EE_NOT_CONFORMABLE);
		pnumL = VPTR(poprTop-1);
		stepL = 1;
		pnumR = VPTR(poprTop);
		stepR = 1;
		nelem = NumElem(poprTop);
		break;
	}

	if (nelem == 1) {	// Result is a scalar
		pnew = &poprTop->uval.xdbl;
		RANK(poprTop) = 0;
	} else {			// Result is an array
		pnew = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pnew);
	}
	TYPE(poprTop) = TNUM;

	while (nelem--) {
		*pnew++ = EvlDyadicScalarNumFun(fun, *pnumL, *pnumR);
		pnumL += stepL;
		pnumR += stepR;
	}
}

static void EvlDyadicMixFun(fun)
{
	double *pnew;
	int nelem;
	int rank;

	// Mixed types. Only = and ≠ are possible and will return all 0's
	if (fun != APL_EQUAL && fun != APL_NOT_EQUAL)
		EvlError(EE_DOMAIN);

	POP(poprTop);

	switch (CMP_TYPES(poprTop-1, poprTop)) {
	case CMP_SCALAR_SCALAR:
		nelem = 1;
		break;
	case CMP_SCALAR_ARRAY:
		nelem = NumElem(poprTop);
		break;
	case CMP_ARRAY_SCALAR:
		rank = RANK(poprTop-1);
		nelem = NumElem(poprTop-1);
		// Copy shape and rank to result
		RANK(poprTop) = rank;
		for (int i = 0; i < rank; ++i)
			SHAPE(poprTop)[i] = SHAPE(poprTop-1)[i];
		break;
	case CMP_ARRAY_ARRAY:
		if (!Conformable(poprTop-1,poprTop))
			EvlError(EE_NOT_CONFORMABLE);
		nelem = NumElem(poprTop);
		break;
	}

	if (nelem == 1) {	// Result is a scalar
		pnew = &poprTop->uval.xdbl;
	} else {			// Result is an array
		pnew = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pnew);
	}
	TYPE(poprTop) = TNUM;
	// All zeros
	memset(pnew, 0, nelem * sizeof(double));
}

static void EvlDyadicFun(int fun, int axis, int axis_type)
{
	int typL, typR;
	int rankL, rankR, rank;

	// Handle non-scalar functions first
	switch (fun) {
	case APL_EPSILON:	// A ∊ B
		FunMembership();
		return;
	case APL_IOTA:		// V ⍳ A
		FunIndexOf();
		return;
	case APL_RHO:		// V ⍴ A
		if (axis_type != AXIS_DEFAULT)
			EvlError(EE_SYNTAX_ERROR);
		FunReshape();
		return;
	case APL_UP_ARROW:	// A ↑ B
		if (axis_type != AXIS_DEFAULT)
			EvlError(EE_SYNTAX_ERROR);
		FunTake();
		return;
	case APL_DOWN_ARROW:// A ↓ B
		if (axis_type != AXIS_DEFAULT)
			EvlError(EE_SYNTAX_ERROR);
		FunDrop();
		return;
	case APL_DOWN_TACK:	// V ⊤ W
		FunEncode();
		return;
	case APL_UP_TACK:	// V ⊥ W
		FunDecode();
		return;
	case APL_DOMINO:	// A ⌹ B
		FunMatDivide();
		return;
	case APL_COMMA:		// A ,[n] B  Default axis = last
	case APL_COMMA_BAR:	// A ⍪[n] B  Default axis = first
		rankL = RANK(poprTop);
		rankR = RANK(poprTop + 1);
		rank = max(rankL,rankR);
		if (axis_type == AXIS_DEFAULT)
			axis = fun == APL_COMMA ? rank - 1 : 0;
		else if (axis >= rank && axis_type != AXIS_LAMINATE)
			EvlError(EE_INVALID_AXIS);
		if (axis < 0) axis = 0;
		FunCatenate(axis, axis_type);
		return;
	case APL_THORN:			// A ⍕ B
		FunFormat2();
		return;
	case APL_QUESTION_MARK:	// A ? B
		FunDeal();
		return;
	case APL_SLASH:			// A /[axis] B  Default axis = last
	case APL_SLASH_BAR:		// A ⌿[axis] B  Default axis = first
		VALIDATE_AXIS(poprTop + 1, APL_SLASH);
		FunCompress(axis);
		return;
	case APL_BACKSLASH:		// A \[axis] B  Default axis = last
	case APL_BACKSLASH_BAR:	// A ⍀[axis] B  Default axis = first
		VALIDATE_AXIS(poprTop + 1, APL_BACKSLASH);
		FunExpand(axis);
		return;
	case APL_CIRCLE_STILE:	// A ⌽[axis] B  Default axis = last
	case APL_CIRCLE_BAR:	// A ⊖[axis] B  Default axis = first
		VALIDATE_AXIS(poprTop + 1, APL_CIRCLE_STILE);
		FunRotate(axis);
		return;
	}

	typL = TYPE(poprTop);
	typR = TYPE(SECOND(poprTop));

	// Numeric functions
	if (typL == TNUM && typR == TNUM) {
		EvlDyadicNumFun(fun);
		return;
	}

	// String functions
	if (typL == TCHR && typR == TCHR) {
		EvlDyadicStrFun(fun);
		return;
	}

	EvlDyadicMixFun(fun);
}

static void EvlMonadicFun(ENV *penv, int fun, int axis, int axis_type)
{
	double *pold, *pnew;
	double num;
	char *pchr;
	int nElem, typ, tmp;

	// Fractional axis is for dyadic lamination only
	if (axis_type == AXIS_LAMINATE)
		EvlError(EE_SYNTAX_ERROR);

	if (axis_type == AXIS_REGULAR && !(fun == APL_CIRCLE_STILE || fun == APL_CIRCLE_BAR))
		EvlError(EE_SYNTAX_ERROR);

	// Handle non-scalar functions first
	switch (fun) {
	case APL_IOTA:			// ⍳N
		FunIota();
		return;
	case APL_RHO:			// ⍴A
		FunShape();
		return;
	case APL_DOMINO:		// ⌹M
		FunMatInverse();
		return;
	case APL_GRADE_DOWN:	// ⍒V
	case APL_GRADE_UP:		// ⍋V
		FunGradeUpDown(fun);
		return;
	case APL_TRANSPOSE:		// ⍉A
		FunTranspose();
		return;
	case APL_CIRCLE_STILE:	// ⌽[axis]A  Default axis = last
	case APL_CIRCLE_BAR:	// ⊖[axis]A  Default axis = first
		VALIDATE_AXIS(poprTop,APL_CIRCLE_STILE);
		FunReverse(axis);
		return;
	case APL_THORN:			// ⍕A
		FunFormat();
		return;
	case APL_HYDRANT:		// ⍎V
		FunExecute(penv);
		return;
	}

	// Not all functions apply to characters/strings
	if (TYPE(poprTop) == TCHR && fun != APL_COMMA)
		EvlError(EE_DOMAIN);

	typ = TYPE(poprTop);

	if (ISSCALAR(poprTop)) {
		if (typ == TNUM) {
			switch (fun) {
			case APL_CIRCLE:
				VNUM(poprTop) *= M_PI;
				break;
			case APL_MINUS:
				VNUM(poprTop) = - VNUM(poprTop);
				break;
			case APL_STAR:
				VNUM(poprTop) = exp(VNUM(poprTop));
				break;
			case APL_QUESTION_MARK:
				tmp = (int)VNUM(poprTop);
				if (tmp) VNUM(poprTop) = (rand() % tmp) + g_origin;
				else VNUM(poprTop) = (double)rand() / (double)RAND_MAX;
				break;
			case APL_STILE:
				VNUM(poprTop) = fabs(VNUM(poprTop));
				break;
			case APL_CIRCLE_STAR:
				if (!(num = VNUM(poprTop)))
					EvlError(EE_DOMAIN);
				VNUM(poprTop) = log(VNUM(poprTop));
				break;
			case APL_UP_STILE:
				VNUM(poprTop) = ceil(VNUM(poprTop));
				break;
			case APL_DOWN_STILE:
				VNUM(poprTop) = floor(VNUM(poprTop));
				break;
			case APL_TIMES:	/* Sign */
				VNUM(poprTop) = SIGN(VNUM(poprTop));
				break;
			case APL_DIV:	/* Inverse */
				if (!(num = VNUM(poprTop)))
					EvlError(EE_DIVIDE_BY_ZERO);
				VNUM(poprTop) = 1.0 / num;
				break;
			case APL_TILDE:	/* Not */
				if (!(num = VNUM(poprTop)))
					VNUM(poprTop) = 1;
				else if (num == 1)
					VNUM(poprTop) = 0;
				else
					EvlError(EE_DOMAIN);
				break;
			case APL_EXCL_MARK:
				num = VNUM(poprTop);
				tmp = (int)num;
				// Not defined for negative integers
				if (tmp < 0 && (double)tmp == num)
					EvlError(EE_DOMAIN);
				VNUM(poprTop) = tgamma(num + 1.0);
				break;
			case APL_COMMA:
				pnew = TempAlloc(sizeof(double), 1);
				*pnew = VNUM(poprTop);
				RANK(poprTop) = 1;
				SHAPE(poprTop)[0] = 1;
				VOFF(poprTop) = WKSOFF(pnew);
				break;
			}
		} else if (typ == TCHR) {
			switch (fun) {
			case APL_COMMA:
				pchr = TempAlloc(sizeof(char), 1);
				*pchr = VCHR(poprTop);
				RANK(poprTop) = 1;
				SHAPE(poprTop)[0] = 1;
				VOFF(poprTop) = WKSOFF(pchr);
				break;
			}
		}
	} else {
		nElem = NumElem(poprTop);
		// + doesn't change the array
		// , just changes the descriptor
		// All other functions need to allocate a new array
		if (fun != APL_PLUS && fun != APL_COMMA) {
			pold  = VPTR(poprTop);
			pnew  = TempAlloc(sizeof(double), nElem);
			VOFF(poprTop) = WKSOFF(pnew);
		}
		switch (fun) {
		case APL_CIRCLE:
			while (nElem--)
				*pnew++ = *pold++ * M_PI;
			break;
		case APL_COMMA:
			RANK(poprTop) = 1;
			SHAPE(poprTop)[0] = nElem;
			break;
		case APL_MINUS:
			while (nElem--)
				*pnew++ = -*pold++;
			break;
		case APL_STAR:
			while (nElem--)
				*pnew++ = exp(*pold++);
			break;
		case APL_QUESTION_MARK:
			while (nElem--) {
				tmp = (int)*pold++;
				if (tmp) *pnew++ = (rand() % tmp) + g_origin;
				else *pnew++ = (double)rand() / (double)RAND_MAX;
			}
			break;
		case APL_STILE:
			while (nElem--) {
				num = *pold++;
				*pnew++ = fabs(num);
			}
			break;
		case APL_CIRCLE_STAR:
			while (nElem--) {
				if (!(num = *pold++))
					EvlError(EE_DOMAIN);
				*pnew++ = log(num);
			}
			break;
		case APL_UP_STILE:
			while (nElem--) {
				num = *pold++;
				*pnew++ = ceil(num);
			}
			break;
		case APL_DOWN_STILE:
			while (nElem--) {
				num = *pold++;
				*pnew++ = floor(num);
			}
			break;
		case APL_TIMES:	/* Sign */
			while (nElem--) {
				num = *pold++;
				*pnew++ = SIGN(num);
			}
			break;
		case APL_DIV:	/* Inverse */
			while (nElem--) {
				num = *pold++;
				if (!num)
					EvlError(EE_DIVIDE_BY_ZERO);
				*pnew++ = 1.0 / num;
			}
			break;
		case APL_TILDE:	/* Not */
			while (nElem--) {
				if (!(num = *pold++))
					*pnew++ = 1;
				else if (num == 1)
					*pnew++ = 0;
				else
					EvlError(EE_DOMAIN);
			}
			break;
		case APL_EXCL_MARK:
			while (nElem--) {
				num = *pold++;
				tmp = (int)num;
				// Not defined for negative integers
				if (tmp < 0 && (double)tmp == num)
					EvlError(EE_DOMAIN);
				*pnew++ = tgamma(num + 1.0);
			}
			break;
		}
	}
}

static double Binomial(double x, double y)
{
	// X!Y ←→ (!Y)÷(!X)×!Y-X
	int tmp;

	// Not defined for negative integers
	tmp = (int)x;
	if (tmp < 0 && (double)tmp == x)
		EvlError(EE_DOMAIN);
	tmp = (int)y;
	if (tmp < 0 && (double)tmp == y)
		EvlError(EE_DOMAIN);

	return tgamma(y + 1.0) / (tgamma(x + 1.0) * tgamma((y-x) + 1.0));
}
static void EvlNumInnerProd(int funL, int funR, ARRAYINFO *L, ARRAYINFO *R)
{
	double *psrL = (double *)L->vptr;
	double *psrR = (double *)R->vptr;
	double *pdst;
	int axis = L->rank - 1;
	int ni = L->nelem / L->shape[axis];	// All but last  L axis
	int nj = R->nelem / R->shape[0];	// All but first R axis
	int nelem = ni * nj;				// # of elements of result
	int R_stride = R->stride[0];

	TYPE(poprTop) = TNUM;
	if (ISSCALAR(poprTop)) {			// Result is a scalar
		assert(nelem == 1);
		pdst = &poprTop->uval.xdbl;
	} else {							// Result is an array
		assert(nelem > 1);
		pdst = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pdst);
	}

	for (int i = 0; i < ni; ++i) {
		for (int j = 0; j < nj; ++j) {
			double *pL = psrL + R->shape[0];
			double *pR = psrR + j + R_stride * R->shape[0];
			// Apply funR accross last elements of L and R
			pL -= 1;
			pR -= R_stride;
			double dotprod = EvlDyadicScalarNumFun(funR, *pL, *pR);
			// Continue with remaining elements (starting from 1)
			for (int k = 1; k < R->shape[0]; ++k) {
				double temp;
				// Apply funR across vectors
				pL -= 1;
				pR -= R_stride;
				temp = EvlDyadicScalarNumFun(funR, *pL, *pR);
				// Apply funL to accumulate
				dotprod = EvlDyadicScalarNumFun(funL, temp, dotprod);
			}
			*pdst++ = dotprod;
		}
		psrL += L->shape[axis];
	}
}

static void EvlStrInnerProd(int funL, int funR, ARRAYINFO *L, ARRAYINFO *R)
{
	char *psrL = (char *)L->vptr;
	char *psrR = (char *)R->vptr;
	double *pdst;						// Result is numeric
	int axis = L->rank - 1;
	int ni = L->nelem / L->shape[axis];	// All but last  L axis
	int nj = R->nelem / R->shape[0];	// All but first R axis
	int nelem = ni * nj;				// # of elements of result
	int R_stride = R->stride[0];

	// Only these functions can be applied to characters
	if (funR != APL_EQUAL && funR != APL_NOT_EQUAL)
		EvlError(EE_DOMAIN);

	TYPE(poprTop) = TNUM;
	if (ISSCALAR(poprTop)) {			// Result is a scalar
		assert(nelem == 1);
		pdst = &poprTop->uval.xdbl;
	} else {							// Result is an array
		assert(nelem > 1);
		pdst = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pdst);
	}

	for (int i = 0; i < ni; ++i) {
		for (int j = 0; j < nj; ++j) {
			char *pL = psrL + R->shape[0];
			char *pR = psrR + j + R_stride * R->shape[0];
			double dotprod;
			// Apply funR accross last elements of L and R
			pL -= 1;
			pR -= R_stride;
			switch (funR) {
			case APL_EQUAL:
				dotprod = *pL == *pR;
				break;
			case APL_NOT_EQUAL:
				dotprod = *pL != *pR;
				break;
			}
			// Continue with remaining elements (starting from 1)
			for (int k = 1; k < R->shape[0]; ++k) {
				double temp;
				// Apply funR across vectors
				pL -= 1;
				pR -= R_stride;
				switch (funR) {
				case APL_EQUAL:
					temp = *pL == *pR;
					break;
				case APL_NOT_EQUAL:
					temp = *pL != *pR;
					break;
				}
				// Apply funL to accumulate
				dotprod = EvlDyadicScalarNumFun(funL, temp, dotprod);
			}
			*pdst++ = dotprod;
		}
		psrL += L->shape[axis];
	}
}

static void EvlInnerProd(int funL, int funR)
{
	ARRAYINFO L;	// Left argument
	ARRAYINFO R;	// Right argument

	// A funL . funR B

	// Left argument
	ArrayInfo(&L);

	// Right argument
	POP(poprTop);
	ArrayInfo(&R);

	// Length of last axis of L must be the same as the first axis of R
	int axis = L.rank - 1;
	if (L.shape[axis] != R.shape[0])
		EvlError(EE_LENGTH);
	
	// Prepare result
	RANK(poprTop) = L.rank + R.rank - 2;
	// Concatenate the shapes of the arguments
	for (int i = 0; i < L.rank - 1; ++i)
		SHAPE(poprTop)[i] = L.shape[i];
	for (int i = 1, j = L.rank - 1; i < R.rank; ++i, ++j)
		SHAPE(poprTop)[j] = R.shape[i];

	// Generic case
	if (funL != APL_PLUS || funR != APL_TIMES) {
		if (L.type == TNUM && R.type == TNUM) {
			EvlNumInnerProd(funL, funR, &L, &R);
			return;
		}
		if (L.type == TCHR && R.type == TCHR) {
			EvlStrInnerProd(funL, funR, &L, &R);
			return;
		}
	}

	if (L.type != TNUM || R.type != TNUM)
		EvlError(EE_DOMAIN);

	// Common case: L +.× R
	double *psrL = (double *)L.vptr;
	double *psrR = (double *)R.vptr;
	double *pdst;
	int ni = L.nelem / L.shape[axis];	// All but last  L axis
	int nj = R.nelem / R.shape[0];		// All but first R axis
	int nelem = ni * nj;				// # of elements of result
	int R_stride = R.stride[0];

	TYPE(poprTop) = TNUM;
	if (ISSCALAR(poprTop)) {			// Result is a scalar
		assert(nelem == 1);
		pdst = &poprTop->uval.xdbl;
	} else {							// Result is an array
		assert(nelem > 1);
		pdst = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pdst);
	}

	for (int i = 0; i < ni; ++i) {
		for (int j = 0; j < nj; ++j) {
			double *pL = psrL;
			double *pR = psrR + j;
			double dotprod = 0;
			for (int k = 0; k < R.shape[0]; ++k) {
				dotprod += *pL * *pR;
				pL += 1;
				pR += R_stride;
			}
			*pdst++ = dotprod;
		}
		psrL += L.shape[axis];
	}
}

static void EvlNumOuterProd(int fun, ARRAYINFO *L, ARRAYINFO *R)
{
	double *pdst = TempAlloc(sizeof(double), L->nelem * R->nelem);
	VOFF(poprTop) = WKSOFF(pdst);

	double *pL = (double *)L->vptr;
	double *pR;
	for (int i = 0; i < L->nelem; ++i) {
		double *pR = (double *)R->vptr;
		double numL = *pL++;
		for (int j = 0; j < R->nelem; ++j) {
			*pdst++ = EvlDyadicScalarNumFun(fun, numL, *pR++);
		}
	}
}

static void EvlStrOuterProd(int fun, ARRAYINFO *L, ARRAYINFO *R)
{
	// Result will be a boolean array
	double *pdst = TempAlloc(sizeof(double), L->nelem * R->nelem);
	VOFF(poprTop) = WKSOFF(pdst);

	char *pL = (char *)L->vptr;
	char *pR;
	for (int i = 0; i < L->nelem; ++i) {
		char *pR = (char *)R->vptr;
		char argL = *pL++;
		for (int j = 0; j < R->nelem; ++j) {
			char argR = *pR++;
			switch (fun) {
			case APL_EQUAL:
				*pdst++ = argL == argR;
				break;
			case APL_NOT_EQUAL:
				*pdst++ = argL != argR;
				break;
			}
		}
	}
}

static void EvlOuterProd(int fun)
{
	ARRAYINFO L;	// Left argument
	ARRAYINFO R;	// Right argument

	// A ∘. fun B

	// Left argument
	ArrayInfo(&L);

	// Right argument
	POP(poprTop);
	ArrayInfo(&R);

	// Prepare result
	RANK(poprTop) = L.rank + R.rank;
	TYPE(poprTop) = TNUM;
	// Concatenate the shapes of the arguments
	for (int i = 0; i < L.rank; ++i)
		SHAPE(poprTop)[i] = L.shape[i];
	for (int i = 0, j = L.rank; i < R.rank; ++i, ++j)
		SHAPE(poprTop)[j] = R.shape[i];

	if (L.type == TNUM && R.type == TNUM) {
		EvlNumOuterProd(fun, &L, &R);
		return;
	}

	if (L.type == TCHR && R.type == TCHR) {
		EvlStrOuterProd(fun, &L, &R);
		return;
	}

	EvlError(EE_DOMAIN);
}

static void FunShape(void)
{
	double *pnew;
	int rank;

	if (ISSCALAR(poprTop)) {
		// Null vector
		TYPE(poprTop) = TNUM;
		RANK(poprTop) = 1;
		SHAPE(poprTop)[0] = 0;
		return;
	}

	rank = RANK(poprTop);
	pnew = TempAlloc(sizeof(double), rank);
	VOFF(poprTop) = WKSOFF(pnew);
	TYPE(poprTop) = TNUM;
	RANK(poprTop) = 1;

	for (int i = 0; i < rank; ++i)
		*pnew++ = SHAPE(poprTop)[i];

	SHAPE(poprTop)[0] = rank;
}

static void FunSystem1(int fun)
{
	switch (fun) {
	case SYS_IDENT:
		SysIdent();
		break;
	case SYS_RREF:
		SysRref();
		break;
	}
}

static void FunReshape(void)
{
	aplshape	shape[MAXDIM];	/* Desired shape */
	int nElemN, nElemO;		/* Number of elements, new and old */
	int rankN;				/* New rank */
	int i, j;
	int nNum;
	double *pdbl;

	// V ⍴ A

	// Left argument must be a numeric vector
	if (TYPE(poprTop) != TNUM)
		EvlError(EE_DOMAIN);

	// A number is treated as a vector of one element
	if (ISSCALAR(poprTop)) {
		rankN = 1;
		shape[0] = nElemN = (int)VNUM(poprTop);
	} else {
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);
		rankN = NumElem(poprTop);
		if (rankN > MAXDIM)
			EvlError(EE_DOMAIN);
		pdbl = VPTR(poprTop);
		nElemN = 1;
		for (i = 0; i < rankN; ++i) {
			if ((nNum = (int)*pdbl++) < 0 || nNum > MAXIND)
				EvlError(EE_DOMAIN);
			shape[i] = (char)nNum;
			nElemN *= nNum;
		}
	}

	POP(poprTop);

	// Right argument
	if (ISSCALAR(poprTop)) {	// Right argument is a scalar
		if (TYPE(poprTop) == TNUM) {
			double num = VNUM(poprTop);
			double *pnew = TempAlloc(sizeof(double), nElemN);
			VOFF(poprTop) = WKSOFF(pnew);
			for (i = 1; i <= nElemN; ++i)
				*pnew++ = num;
		} else if (TYPE(poprTop) == TCHR) {
			int chr = VCHR(poprTop);
			char *pnew = TempAlloc(sizeof(char), nElemN);
			VOFF(poprTop) = WKSOFF(pnew);
			for (i = 1; i <= nElemN; ++i)
				*pnew++ = chr;
		}
	} else {					// Right argument is an array
		if (TYPE(poprTop) == TNUM) {
			double *pnew, *pold;
			double prot = 0.0;
			nElemO = NumElem(poprTop);
			pold = nElemO ? VPTR(poprTop) : &prot;
			if (nElemN > nElemO) {
				pnew = TempAlloc(sizeof(double), nElemN);
				pdbl = pold;
				VOFF(poprTop) = WKSOFF(pnew);
				for (i = 1, j = 0; i <= nElemN; ++i) {
					*pnew++ = *pdbl++;
					if (++j >= nElemO) {
						pdbl = pold;
						j = 0;
					}
				}
			}
		} else if (TYPE(poprTop) == TCHR) {
			char *pchr, *pnew, *pold;
			char prot = ' ';
			nElemO = NumElem(poprTop);
			pold = nElemO ? VPTR(poprTop) : &prot;
			if (nElemN > nElemO) {
				pnew = TempAlloc(sizeof(char), nElemN);
				pchr = pold;
				VOFF(poprTop) = WKSOFF(pnew);
				for (i = 1, j = 0; i <= nElemN; ++i) {
					*pnew++ = *pchr++;
					if (++j >= nElemO) {
						pchr = pold;
						j = 0;
					}
				}
			}
		}
	}

	memcpy(SHAPE(poprTop), shape, rankN * sizeof(aplshape));
	RANK(poprTop) = rankN;
}

static void FunReverse(int axis)
{
	aplshape	shape[MAXDIM];	// Shape of the argument
	int size[MAXDIM];	// Sizes of axes of the argument
	int super[MAXDIM];	// Number of super-arrays for each axis
	int rank;			// Rank of the argument
	int	is_num;			// 1 if argument is numeric, 0 if character
	int nelem;

	// ⌽[axis]A

	// If argument is not an array, leave it unchanged
	if (!ISARRAY(poprTop))
		return;

	is_num = TYPE(poprTop) == TNUM;
	rank = RANK(poprTop);

	// Fill in shape[] and size[]
	// Calculate nelem
	nelem = 1;
	for (int i = rank - 1; i >= 0; --i) {
		int n = SHAPE(poprTop)[i];
		shape[i] = n;
		size[i] = nelem;
		nelem *= n;
	}
	// If argument is an empty array, leave it unchanged
	if (!nelem)
		return;

	// Fill in super[]
	for (int i = 0, siz = 1; i < rank; ++i) {
		super[i] = siz;
		siz *= shape[i];
	}

	// Set result
	// Same rank and shape as input

	if (is_num) {		// Numbers
		double *pdst;
		double *psrc;
		psrc = (double *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		if (axis == rank - 1) {	// Special case: last axis
			for (int i = 0; i < super[axis]; ++i) {
				psrc += shape[axis];	// Last element + 1 in this axis
				for (int j = 0; j < shape[axis]; ++j)
					*pdst++ = *--psrc;
				psrc += shape[axis];
			}
		} else {				// Generic case
			int copylen = size[axis] * sizeof(double);
			for (int i = 0; i < super[axis]; ++i) {
				psrc += shape[axis] * size[axis]; // Last element + 1 in this axis
				for (int j = 0; j < shape[axis]; ++j) {
					psrc -= size[axis];
					memcpy(pdst, psrc, copylen);
					pdst += size[axis];
				}
				psrc += shape[axis] * size[axis];
			}
		}
	} else {			// Characters
		char *pdst;
		char *psrc;

		psrc = (char *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(char), nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		if (axis == rank - 1) {	// Special case: last axis
			for (int i = 0; i < super[axis]; ++i) {
				psrc += shape[axis];	// Last element + 1 in this axis
				for (int j = 0; j < shape[axis]; ++j)
					*pdst++ = *--psrc;
				psrc += shape[axis];
			}
		} else {				// Generic case
			int copylen = size[axis] * sizeof(char);
			for (int i = 0; i < super[axis]; ++i) {
				psrc += shape[axis] * size[axis]; // Last element + 1 in this axis
				for (int j = 0; j < shape[axis]; ++j) {
					psrc -= size[axis];
					memcpy(pdst, psrc, copylen);
					pdst += size[axis];
				}
				psrc += shape[axis] * size[axis];
			}
		}
	}
}

// A ⌽ B
// A is the rotation array
// B is the source array
typedef struct tagRotIndex {
	aplshape shape[MAXDIM];	// Shape of source array
	int size[MAXDIM];	// Size of source array
	int index[MAXDIM];	// Index of source array
	int rsize[MAXDIM];	// Size of rotation array
	int rank;			// Rank of source array
	int nelem;			// Number of elements in source array
	int axis;			// Rotation axis
	double *rotarray;	// Pointer to rotation array elements
} ROTINDEX;

// Scalars on either side have been dealt with. When we get here
// both sides are guaranteed to be arrays.
// rot ⌽ src
static int CreateRotateIndex(ROTINDEX *prot, DESC *rot, DESC *src, int axis)
{
	assert(ISARRAY(rot));
	assert(ISARRAY(src));

	// The shape of the rotation array must be like the shape of the source
	// array with the rotation axis removed. Therefore its rank is one less
	// than the rank of the source array.
	int rank = RANK(src);
	if (RANK(rot) != rank - 1)
		EvlError(EE_RANK);

	// Check if shapes are compatible
	for (int i = 0, r = 0; i < rank; ++i) {
		prot->index[i] = 0;
		prot->shape[i] = SHAPE(src)[i];
		if (i != axis) {
			if (SHAPE(rot)[r] != prot->shape[i])
				EvlError(EE_LENGTH);
			++r;
		}
	}

	int nelem = 1;
	for (int i = rank - 1; i >= 0; --i) {
		prot->size[i] = nelem;
		nelem *= prot->shape[i];
	}

	for (int i = rank - 2, siz = 1; i >= 0; --i) {
		prot->rsize[i] = siz;
		siz *= SHAPE(rot)[i];
	}

	prot->rank = rank;
	prot->nelem = nelem;
	prot->axis = axis;
	prot->rotarray = VPTR(rot);

	return nelem > 0;
}

static int GetRotateIndex(ROTINDEX *prot)
{
	int ind = 0;
	int indr = 0;

	// Calculate the linear index of the rotation array.
	// Same indices of the source array except for the rotation axis.
	// src[i1; i2; ...; ii; ... in]
	// rot[i1; i2; ... in]
	for (int i = 0, r = 0; i < prot->rank; ++i) {
		if (i != prot->axis) indr += prot->index[i] * prot->rsize[r++];
	}

	// All indices remain the same except for the rotation axis

	// src[i1; i2; ... ii; ...in] -> dst[i1; i2; ir;... in]
	// where:
	//   'r' is the rotation axis
	//   'ii' is the index to be rotated
	//   'ir' is the rotated index
	//   'rotarr' is the rotation array (all indices except 'r')
	// ii -> ir = (ii - rotarr[i1,i2...,in]) % shape[r]

	for (int i = 0; i < prot->rank; ++i) {
		if (i != prot->axis)
			ind += prot->index[i] * prot->size[i];
		else {
			// Calculate rotated index
			int ir = (prot->index[i] - (int)prot->rotarray[indr]);
			if (ir < 0) {
				// Revert to a positive number to avoid the
				// implementation behavior of (n % m) when n < 0.
				ir = (-ir / prot->shape[i] + 1) * prot->shape[i] + ir;
			}
			ind += (ir % prot->shape[i]) * prot->size[i];
		}
	}

	return ind;
}

static int NextRotateIndex(ROTINDEX *prot)
{
	for (int j = prot->rank - 1; j >= 0; --j) {
		if (++prot->index[j] < prot->shape[j])
			return 1;	// Continue

		// Reset this level and backtrack
		prot->index[j] = 0;
	}

	return 0;	// Stop
}

static void FunRotate(int axis)
{
	ROTINDEX rot;		// Rotation indices
	int	is_num;			// 1 if rhs is numeric, 0 if character
	int ind;
	int not_done;

	// A⌽[axis]B

	// A must be numeric
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	POP(poprTop);

	if (ISSCALAR(poprTop-1)) {	// A is a scalar
		// If both A and B are scalars, do nothing
		if (ISSCALAR(poprTop))
			return;
		// Extend A as an array with rank = RANK(B) - 1
		// Same shape as B except for the rotation axis
		DESC *prot = poprTop - 1;
		double rot = VNUM(prot);
		int rank = RANK(poprTop);
		int nelem = 1;
		for (int i = 0, r = 0; i < rank; ++i) {
			int n;
			if (i != axis) {
				n = SHAPE(poprTop)[i];
				SHAPE(prot)[r++] = n;
				nelem *= n;
			}
		}
		double *pdbl = TempAlloc(sizeof(double), nelem);
		for (int i = 0; i < nelem; ++i)
			*(pdbl + i) = rot;
		RANK(prot) = rank - 1;
		VOFF(prot) = WKSOFF(pdbl);
	} else {					// A is an array
		if (!ISARRAY(poprTop))
			EvlError(EE_RANK);
	}

	not_done = CreateRotateIndex(&rot, poprTop-1, poprTop, axis);

	// If this is a null array, do nothing
	if (!rot.nelem)
		return;

	// Set result (same rank and shape as B)

	if (ISNUMBER(poprTop)) {	// Numbers
		double *pdst;
		double *psrc;
		psrc = (double *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(double), rot.nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		while (not_done) {
			ind = GetRotateIndex(&rot);
			*(pdst + ind) = *psrc++;
			not_done = NextRotateIndex(&rot);
		}
	} else {					// Characters
		char *pdst;
		char *psrc;
		psrc = (char *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(char), rot.nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		while (not_done) {
			ind = GetRotateIndex(&rot);
			*(pdst + ind) = *psrc++;
			not_done = NextRotateIndex(&rot);
		}
	}
}

static void FunCatenate(int axis, int axis_type)
{
	ARRAYINFO L;	// Left argument
	ARRAYINFO R;	// Right argument
	int cpyshp = 0;	// 1 if shape of L must be copied to result
	int scalar = 0;	// 1 if either argument is a scalar that was extended

	// A,[axis]B

	// Left argument
	ArrayInfo(&L);

	// Right argument
	POP(poprTop);
	ArrayInfo(&R);

	if (L.type != R.type)	// Sorry, no boxed arrays (yet)...
		EvlError(EE_DOMAIN);// Cannot mix numbers and characters

	// To be conformable for catenation both arrays must have the same
	// rank and their shapes can differ only in the catenation axis.
	if (L.rank - R.rank == 1) {				// Left > Right
		ExtendArray(&R, axis);
		cpyshp = 1;	// Copy left shape to result
	} else if (R.rank - L.rank == 1) {		// Right > Left
		ExtendArray(&L, axis);
	}

	if (L.rank == R.rank) {					// Left == Right
		if (axis_type == AXIS_LAMINATE) {
			ExtendArray(&L, axis);
			ExtendArray(&R, axis);
		}
		for (int i = 0; i < L.rank; ++i) {
			if (i != axis && L.shape[i] != R.shape[i])
				EvlError(EE_LENGTH);
			if (axis_type == AXIS_LAMINATE)
				SHAPE(poprTop)[i] = L.shape[i];
		}
	} else if (L.rank == 1 && L.nelem == 1) {
		// Extend scalar to array
		ExtendScalar(&R, &L, axis);
		scalar = 1;
	} else if (R.rank == 1 && R.nelem == 1) {
		// Extend scalar to array
		ExtendScalar(&L, &R, axis);
		scalar = 1;
		cpyshp = 1;
	} else
		EvlError(EE_RANK);

	// Set result
	assert(L.rank == R.rank);
	RANK(poprTop) = max(L.rank,R.rank);
	if (cpyshp) {
		for (int i = 0; i < L.rank; ++i)
			SHAPE(poprTop)[i] = L.shape[i];
		RANK(poprTop) = L.rank;
	}
	SHAPE(poprTop)[axis] = L.shape[axis] + R.shape[axis];

	if (L.type == TNUM) {	// Numbers
		double *psrL = (double *)L.vptr;
		double *psrR = (double *)R.vptr;
		double *pdst = TempAlloc(sizeof(double), L.nelem + R.nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		if (!axis && !scalar) {						// Special case: first axis
			// Copy whole left + right
			memcpy(pdst, psrL, L.nelem * sizeof(double));
			memcpy(pdst + L.nelem, psrR, R.nelem * sizeof(double));
		} else if (axis == L.rank - 1 && !scalar) {// Special case: last axis
			for (int i = 0; i < L.super[axis]; ++i) {
				// Alternate rows from left and right
				memcpy(pdst, psrL, L.shape[axis] * sizeof(double));
				pdst += L.shape[axis];
				psrL += L.shape[axis];
				memcpy(pdst, psrR, R.shape[axis] * sizeof(double));
				pdst += R.shape[axis];
				psrR += R.shape[axis];
			}
		} else {						// Generic case
			int L_stride = L.stride[axis];
			int R_stride = R.stride[axis];
			int L_inner  = L.shape[axis] * L_stride;
			int R_inner  = R.shape[axis] * R_stride;
			for (int i = 0; i < L.super[axis]; ++i) {
				// Copy left "column"
				for (int j = 0; j < L.size[axis]; ++j) {
					double *pL = psrL;
					double *pd = pdst;
					for (int k = 0; k < L.shape[axis]; ++k) {
						*pd = *pL;
						pd += L_stride;
						pL += L_stride;
					}
					psrL += L.step;	// scalar ? 0 : 1
					++pdst;
				}
				psrL += L_inner - L_stride;
				pdst += L_inner - L_stride;
				// Copy right "column"
				for (int j = 0; j < R.size[axis]; ++j) {
					double *pR = psrR;
					double *pd = pdst;
					for (int k = 0; k < R.shape[axis]; ++k) {
						*pd = *pR;
						pd += R_stride;
						pR += R_stride;
					}
					psrR += R.step;	// scalar ? 0 : 1
					++pdst;
				}
				psrR += R_inner - R_stride;
				pdst += R_inner - R_stride;
			}
		}
	} else {		// Characters
		char *psrL = L.vptr;
		char *psrR = R.vptr;
		char *pdst = TempAlloc(sizeof(char), L.nelem + R.nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		if (!axis && !scalar) {						// Special case: first axis
			// Copy whole left + right
			memcpy(pdst, psrL, L.nelem * sizeof(char));
			memcpy(pdst + L.nelem, psrR, R.nelem * sizeof(char));
		} else if (axis == L.rank - 1 && !scalar) {// Special case: last axis
			for (int i = 0; i < L.super[axis]; ++i) {
				// Alternate rows from left and right
				memcpy(pdst, psrL, L.shape[axis] * sizeof(char));
				pdst += L.shape[axis];
				psrL += L.shape[axis];
				memcpy(pdst, psrR, R.shape[axis] * sizeof(char));
				pdst += R.shape[axis];
				psrR += R.shape[axis];
			}
		} else {									// Generic case
			int L_stride = L.stride[axis];
			int R_stride = R.stride[axis];
			int L_inner  = L.shape[axis] * L_stride;
			int R_inner  = R.shape[axis] * R_stride;
			for (int i = 0; i < L.super[axis]; ++i) {
				// Copy left "column"
				for (int j = 0; j < L.size[axis]; ++j) {
					char *pL = psrL;
					char *pd = pdst;
					for (int k = 0; k < L.shape[axis]; ++k) {
						*pd = *pL;
						pd += L_stride;
						pL += L_stride;
					}
					psrL += L.step;	// scalar ? 0 : 1				
					++pdst;
				}
				psrL += L_inner - L_stride;
				pdst += L_inner - L_stride;
				// Copy right "column"
				for (int j = 0; j < R.size[axis]; ++j) {
					char *pR = psrR;
					char *pd = pdst;
					for (int k = 0; k < R.shape[axis]; ++k) {
						*pd = *pR;
						pd += R_stride;
						pR += R_stride;
					}
					psrR += R.step;	// scalar ? 0 : 1
					++pdst;
				}
				psrR += R_inner - R_stride;
				pdst += R_inner - R_stride;
			}
		}
	}
}

static void FunCompress(int axis)
{
	int	*mask;			// Copy of the left argument
	int	shape[MAXDIM];	// Shape of the right argument
	int size[MAXDIM];	// Sizes of axes of the right argument
	int super[MAXDIM];	// Number of super-arrays for each axis
	int masklen;		// # of elements in the left argument (mask)
	int shape_axis;		// # of elements in the compressed axis
	int nelem_dst;		// # of elements in result
	int rank;			// Rank of the right argument
	int	rhs_is_num;		// 1 if rhs is numeric, 0 if character
	int lhs_is_scalar;	// 1 if lhs is a scalar
	double scalar_num;	// Right argument is a scalar
	char scalar_char;
	char *parr;			// Generic pointer to source array
	int incr;			// Source pointer increment
	int n;

	// V/[axis]A

	// V must be a numeric vector or scalar
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	if (ISARRAY(poprTop)) {
		lhs_is_scalar = 0;
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);

		masklen = NumElem(poprTop);
		mask = TempAlloc(sizeof(int), masklen);
		double *pdbl = VPTR(poprTop);

		shape_axis = 0;
		for (int i = 0; i < masklen; ++i) {
			n = (int)*pdbl;
			if ((double)n != *pdbl)		// Must be an integer
				EvlError(EE_DOMAIN);
			mask[i] = n;
			shape_axis += abs(n);
			++pdbl;
		}
	} else {
		lhs_is_scalar = 1;	// Expand it later
		mask = TempAlloc(sizeof(int), 1);
		mask[0] = (int)VNUM(poprTop);
	}

	POP(poprTop);

	// Right argument (source)
	rhs_is_num = ISNUMBER(poprTop);

	if (ISARRAY(poprTop)) {	// rhs is an array
		rank = RANK(poprTop);

		// Expand lhs if necessary
		if (lhs_is_scalar) {
			int m = mask[0];
			masklen = SHAPE(poprTop)[axis];
			shape_axis = m * masklen;
			for (int i = 1; i < masklen; ++i)
				mask[i] = m;
		}

		// Fill in shape[] and size[]
		// Calculate nelem_dst
		nelem_dst = 1;
		for (int i = rank - 1, siz = 1; i >= 0; --i) {
			n = SHAPE(poprTop)[i];
			nelem_dst *= i == axis ? shape_axis : n;
			shape[i] = n;
			size[i] = siz;
			siz *= n;
		}
		// Fill in super[]
		for (int i = 0, siz = 1; i < rank; ++i) {
			super[i] = siz;
			siz *= shape[i];
		}
		if (masklen != shape[axis])
			EvlError(EE_LENGTH);
	
		parr = VPTR(poprTop);
		incr = 1;	// Regular increment for psrc
	} else {				// rhs is a scalar
		axis = 0;
		rank = 1;
		size[0] = 1;
		super[0] = 1;

		if (lhs_is_scalar) {
			shape[0] = 1;
			nelem_dst = shape_axis = abs(mask[0]);
		} else {
			shape[0] = masklen;
			nelem_dst = shape_axis;
		}

		if (rhs_is_num) {
			scalar_num = VNUM(poprTop);
			parr = (char *)&scalar_num;
		} else {
			scalar_char = VCHR(poprTop);
			parr = &scalar_char;
		}
		RANK(poprTop) = 1;
		incr = 0;	// Don't increment psrc (use the same scalar value)
	}


	// Set result
	VOFF(poprTop) = 0;
	SHAPE(poprTop)[axis] = shape_axis;

	if (nelem_dst) {
		if (rhs_is_num) {		// Numbers
			double *pdst;
			double *psrc;
			double elem;
			psrc = (double *)parr;
			pdst = TempAlloc(sizeof(double), nelem_dst);
			VOFF(poprTop) = WKSOFF(pdst);

			if (axis == rank - 1) {	// Special case: last axis
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < shape[axis]; ++j) {
						n = mask[j];
						if (n < 0) { n = -n; elem = 0; }
						else { elem = *psrc; }
						for (int k = 0; k < n; ++k)	// Replicate 1 element
							*pdst++ = elem;
						psrc += incr;
					}
				}
			} else {				// Generic case
				int copylen = size[axis] * sizeof(double);
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < shape[axis]; ++j) {
						n = mask[j];
						if (n > 0) {
							for (int k = 0; k < n; ++k) {	// Replicate 1 sub-array
								memcpy(pdst, psrc, copylen);
								pdst = (double *)((char *)pdst + copylen);
							}
						} else if (n < 0) {
							n = (-n) * copylen;
							memset(pdst, 0, n);
							pdst = (double *)((char *)pdst + n);
						}
						psrc = (double *)((char *)psrc + copylen);
					}
				}
			}
		} else {			// Characters
			char *pdst;
			char *psrc;
			int elem;
			psrc = parr;
			pdst = TempAlloc(sizeof(char), nelem_dst);
			VOFF(poprTop) = WKSOFF(pdst);

			if (axis == rank - 1) {	// Special case: last axis
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < shape[axis]; ++j) {
						n = mask[j];
						if (n < 0) { n = -n; elem = ' '; }
						else { elem = *psrc; }
						for (int k = 0; k < n; ++k)	// Replicate 1 element
							*pdst++ = elem;
						psrc += incr;
					}
				}
			} else {				// Generic case
				int copylen = size[axis] * sizeof(char);
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < shape[axis]; ++j) {
						n = mask[j];
						if (n > 0) {
							for (int k = 0; k < n; ++k) {	// Replicate 1 sub-array
								memcpy(pdst, psrc, copylen);
								pdst = pdst + copylen;
							}
						} else if (n < 0) {
							n = (-n) * copylen;
							memset(pdst, ' ', n);
							pdst = pdst + n;
						}
						psrc = psrc + copylen;
					}
				}
			}
		}
	}	
}

static void FunExpand(int axis)
{
	int	*mask;			// Copy of the left argument
	int	shape[MAXDIM];	// Shape of the right argument
	int size[MAXDIM];	// Sizes of axes of the right argument
	int super[MAXDIM];	// Number of super-arrays for each axis
	int masklen;		// # of elements in the left argument (mask)
	int shape_axis;		// # of elements in the expanded axis
	int nelem_dst;		// # of elements in result
	int num_pos;		// # of positive mask elements
	int rank;			// Rank of the right argument
	int	rhs_is_num;		// 1 if rhs is numeric, 0 if character
	int lhs_is_scalar;	// 1 if lhs is a scalar
	double scalar_num;	// Right argument is a scalar
	char scalar_char;
	char *parr;			// Generic pointer to source array
	int incr;			// Source pointer increment
	int n;

	// V\[axis]A

	// V must be a numeric vector or scalar
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	if (ISARRAY(poprTop)) {
		lhs_is_scalar = 0;
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);

		masklen = NumElem(poprTop);
		mask = TempAlloc(sizeof(int), masklen);
		double *pdbl = VPTR(poprTop);

		shape_axis = 0;
		num_pos = 0;
		for (int i = 0; i < masklen; ++i) {
			n = (int)*pdbl;
			if ((double)n != *pdbl)		// Must be an integer
				EvlError(EE_DOMAIN);
			if (n > 0)
				++num_pos;
			if (!n)						// Treat 0 like -1
				n = -1;					// Insert 1 zero (or ' ')
			shape_axis += abs(n);
			mask[i] = n;
			++pdbl;
		}
	} else {
		lhs_is_scalar = 1;	// rhs must also be a scalar
		masklen = 1;
		mask = TempAlloc(sizeof(int), masklen);
		n = (int)VNUM(poprTop);
		if (!n)							// Treat 0 like -1
			n = -1;						// Insert 1 zero (or ' ')
		mask[0] = n;
		num_pos = n > 0 ? 1 : 0;
		nelem_dst = shape_axis = abs(mask[0]);
	}

	POP(poprTop);

	// Right argument (source)
	rhs_is_num = ISNUMBER(poprTop);

	if (ISARRAY(poprTop)) {	// rhs is an array
		rank = RANK(poprTop);

		if (axis >= rank)
			EvlError(EE_RANK);

		// Fill in shape[] and size[]
		// Calculate nelem_dst
		nelem_dst = 1;
		for (int i = rank - 1, siz = 1; i >= 0; --i) {
			n = SHAPE(poprTop)[i];
			nelem_dst *= i == axis ? shape_axis : n;
			shape[i] = n;
			size[i] = siz;
			siz *= n;
		}
		// Fill in super[]
		for (int i = 0, siz = 1; i < rank; ++i) {
			super[i] = siz;
			siz *= shape[i];
		}

		if (shape[axis] > 1 && num_pos != shape[axis])
			EvlError(EE_LENGTH);

		if (lhs_is_scalar) {
			if (!((!shape[axis] && !num_pos) ||
				  (shape[axis] == 1 && num_pos)  ))
			EvlError(EE_LENGTH);
		}
	
		parr = VPTR(poprTop);
		incr = 1;	// Regular increment for psrc
	} else {				// rhs is a scalar
		axis = 0;
		rank = 1;
		size[0] = 1;
		super[0] = 1;
		shape[0] = 1;

		if (!lhs_is_scalar) {
			nelem_dst = 0;
			for (int i = 0; i < masklen; ++i) {
				n = mask[i];
				if (n) nelem_dst += abs(n);
				else ++nelem_dst;
			}
		}

		if (rhs_is_num) {
			scalar_num = VNUM(poprTop);
			parr = (char *)&scalar_num;
		} else {
			scalar_char = VCHR(poprTop);
			parr = &scalar_char;
		}
		RANK(poprTop) = 1;
		incr = 0;	// Don't increment psrc (use the same scalar value)
	}

	// Set result
	VOFF(poprTop) = 0;
	SHAPE(poprTop)[axis] = shape_axis;

	if (nelem_dst) {
		if (rhs_is_num) {		// Numbers
			double *pdst;
			double *psrc;
			double elem;
			psrc = (double *)parr;
			pdst = TempAlloc(sizeof(double), nelem_dst);
			VOFF(poprTop) = WKSOFF(pdst);

			if (axis == rank - 1) {	// Special case: last axis
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < masklen; ++j) {
						n = mask[j];
						if (n < 0) { n = -n; elem = 0; }
						else { elem = *psrc; psrc += incr; }
						for (int k = 0; k < n; ++k)	// Replicate 1 element
							*pdst++ = elem;
					}
				}
			} else {				// Generic case
				int copylen = size[axis] * sizeof(double);
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < masklen; ++j) {
						n = mask[j];
						if (n > 0) {
							for (int k = 0; k < n; ++k) {	// Replicate 1 sub-array
								memcpy(pdst, psrc, copylen);
								pdst = (double *)((char *)pdst + copylen);
							}
							psrc = (double *)((char *)psrc + copylen);
						} else {
							n = (-n) * copylen;
							memset(pdst, 0, n);
							pdst = (double *)((char *)pdst + n);
						}
					}
				}
			}
		} else {			// Characters
			char *pdst;
			char *psrc;
			int elem;
			psrc = parr;
			pdst = TempAlloc(sizeof(char), nelem_dst);
			VOFF(poprTop) = WKSOFF(pdst);

			if (axis == rank - 1) {	// Special case: last axis
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < masklen; ++j) {
						n = mask[j];
						if (n < 0) { n = -n; elem = ' '; }
						else { elem = *psrc; psrc += incr; }
						for (int k = 0; k < n; ++k)	// Replicate 1 element
							*pdst++ = elem;
					}
				}
			} else {				// Generic case
				int copylen = size[axis] * sizeof(char);
				for (int i = 0; i < super[axis]; ++i) {
					for (int j = 0; j < masklen; ++j) {
						n = mask[j];
						if (n > 0) {
							for (int k = 0; k < n; ++k) {	// Replicate 1 sub-array
								memcpy(pdst, psrc, copylen);
								pdst = pdst + copylen;
							}
							psrc = psrc + copylen;
						} else {
							n = (-n) * copylen;
							memset(pdst, ' ', n);
							pdst = pdst + n;
						}
					}
				}
			}
		}
	}	
}

static void	FunDeal(void)
{
	ARRAYINFO L;
	ARRAYINFO R;
	double num;

	// L?R
	// nelem ? total

	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	if (L.nelem != 1 || R.nelem != 1)
		EvlError(EE_LENGTH);

	if (L.type != TNUM || R.type != TNUM)
		EvlError(EE_DOMAIN);

	// Arguments must be integers
	num = *(double *)L.vptr;
	int nelem = (int)num;
	if ((double)nelem != num)
		EvlError(EE_DOMAIN);

	if (nelem > MAXIND)	// We don't support a vector this big
		EvlError(EE_LENGTH);
	
	num = *(double *)R.vptr;
	int total = (int)num;
	if ((double)total != num)
		EvlError(EE_DOMAIN);

	// Arguments must be non-negative and L<=R
	if (nelem < 0 || total < 0 || nelem > total)
		EvlError(EE_DOMAIN);

	// Set result
	RANK(poprTop) = 1;
	SHAPE(poprTop)[0] = nelem;

	if (!nelem)	// Zero elements
		return;

	// Allocate a bitmap to mark available slots
	int nbytes = ALIGN(total,8) / 8;
	uint8_t *bits = (uint8_t *)TempAlloc(sizeof(uint8_t), nbytes);
	// 1 = available, 0 = already drawn
	memset(bits, 0xff, nbytes);
	// Fix last byte mask
	int rest = total & 7;
	int mask = 0;
	for (int i = 0; i < rest; ++i)
		mask = (mask << 1) | 1;
	if (mask)
		bits[nbytes-1] = mask;

	// Allocate result vector
	double *pdst = TempAlloc(sizeof(double), nelem);
	VOFF(poprTop) = WKSOFF(pdst);

	// Draw 'nelem' numbers from 'total'
	for (int i = 0; i < nelem; ++i) {
		int tmp = (rand() % total);
		// Check if available
		int ind = tmp / 8;
		int bit = 1 << (tmp % 8);

		while (!(bits[ind] & bit)) { // This loop is guaranteed to end
			// Try next ones, wrap-around if necessary
			if (tmp < total - 1) ++tmp;
			else tmp = 0;
			ind = tmp / 8;
			bit = 1 << (tmp % 8);
		}

		bits[ind] &= ~bit;
		*pdst++ = tmp + g_origin;
	}
}

static void FunDecode()
{
	ARRAYINFO L;
	ARRAYINFO R;

	// L ⊥ R
	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	if (L.type != TNUM || R.type != TNUM)
		EvlError(EE_DOMAIN);

	if (L.rank != 1 || R.rank != 1)
		EvlError(EE_RANK);

	if (L.nelem != R.nelem && L.nelem != 1 && R.nelem != 1)
		EvlError(EE_LENGTH);

	double *pL = (double *)L.vptr + L.step;	// Ignore 1st element
	double *pR = (double *)R.vptr;
	double value = *pR;
	pR += R.step;

	for (int i = 1; i < R.nelem; ++i) {
		value = value * *pL + *pR;
		pL += L.step;
		pR += R.step;
	}

	// Set result (a scalar)
	TYPE(poprTop) = TNUM;
	RANK(poprTop) = 0;
	VNUM(poprTop) = value;
}

static void FunMatDivide(void)
{
	// V ⌹ M

	int nr, nc, len;
	double *vec, *mat;

	// Left argument must be a numeric vector
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (!ISARRAY(poprTop) || RANK(poprTop) != 1)
		EvlError(EE_RANK);

	len = SHAPE(poprTop)[0];
	vec = VPTR(poprTop);

	POP(poprTop);

	// Right argument must be a numeric matrix
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (!ISARRAY(poprTop) || RANK(poprTop) != 2)
		EvlError(EE_RANK);

	nr = SHAPE(poprTop)[0];
	nc = SHAPE(poprTop)[1];

	// Vector length must be equal to the number of rows in the matrix
	// The matrix must be square
	if (nr != len || nr != nc)
		EvlError(EE_LENGTH);

	// Allocate augmented matrix
	mat = TempAlloc(sizeof(double), nr * (nc + 1));
	// Copy matrix to new array row by row adding vector at the end
	double *pdst = mat;
	double *psrc = VPTR(poprTop);
	for (int i = 0; i < nr; ++i) {
		memcpy(pdst, psrc, nc * sizeof(double));
		pdst += nc;
		psrc += nc;
		*pdst++ = vec[i];
	}

	VOFF(poprTop) = WKSOFF(mat);
	SHAPE(poprTop)[1] = nc + 1;

	// Transform augmented matrix into its Reduced Row Echelon Form
	if (MatRref(mat, nr, nc + 1) < nr)
		EvlError(EE_DOMAIN);	// Singular matrix

	// Get result from last column
	pdst = TempAlloc(sizeof(double),nr);
	psrc = mat + nc;
	VOFF(poprTop) = WKSOFF(pdst);
	for (int i = 0; i < nr; ++i) {
		*pdst++ = *psrc;
		psrc += nc + 1;
	}
	RANK(poprTop) = 1;
	SHAPE(poprTop)[0] = nr;
}

static void FunMatInverse(void)
{
	// ⌹ M

	int nr, nc, len;
	double *mat;

	// Argument must be a numeric square matrix
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (!ISARRAY(poprTop) || RANK(poprTop) != 2)
		EvlError(EE_RANK);

	nr = SHAPE(poprTop)[0];
	nc = SHAPE(poprTop)[1];

	// Must be square
	if (nr != nc)
		EvlError(EE_LENGTH);

	// Allocate augmented matrix
	mat = TempAlloc(sizeof(double), nr * (nc * 2));
	// Copy matrix to new array row by row adding identity matrix at the end
	double *pdst = mat;
	double *psrc = VPTR(poprTop);
	for (int i = 0; i < nr; ++i) {
		memcpy(pdst, psrc, nc * sizeof(double));
		pdst += nc;
		psrc += nc;
		memset(pdst, 0, nc * sizeof(double));
		pdst[i] = 1.0;
		pdst += nc;
	}

	VOFF(poprTop) = WKSOFF(mat);
	SHAPE(poprTop)[1] = nc * 2;

	if (MatRref(mat, nr, nc * 2) < nr)
		EvlError(EE_DOMAIN);

	// Get result from last column
	pdst = TempAlloc(sizeof(double),nr * nc);
	psrc = mat + nc;
	VOFF(poprTop) = WKSOFF(pdst);
	for (int i = 0; i < nr; ++i) {
		memcpy(pdst, psrc, nc * sizeof(double));
		psrc += nc * 2;
		pdst += nc;
	}
	RANK(poprTop) = 2;
	SHAPE(poprTop)[0] = nr;
	SHAPE(poprTop)[1] = nc;
}

static void FunEncode()
{
	ARRAYINFO L;
	ARRAYINFO R;

	// L ⊤ R
	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	if (L.type != TNUM || R.type != TNUM)
		EvlError(EE_DOMAIN);

	if (L.rank != 1 || R.nelem != 1)
		EvlError(EE_RANK);

	int digits = L.nelem;
	double num = *(double *)R.vptr;
	double *pL = (double *)L.vptr;
	double *pdst = TempAlloc(sizeof(double), L.nelem);

	// Prepare result (a vector)
	RANK(poprTop) = 1;
	SHAPE(poprTop)[0] = digits;
	VOFF(poprTop) = WKSOFF(pdst);

	// Work right to left
	pL += digits;
	pdst += digits;
	for (int i = 0; i < digits; ++i) {
		double div = *--pL;
		double rem = fmod(num, div);
		*--pdst = rem;
		num = (num - rem) / div;
	}
}

static void	FunExecute(ENV *penv)
{
	LEXER lex;
	ENV env;

	if (TYPE(poprTop) != TCHR)
		EvlError(EE_DOMAIN);

	if (RANK(poprTop) != 1)
		EvlError(EE_RANK);

	int len = SHAPE(poprTop)[0];
	char *src  = (char *)VPTR(poprTop);
	POP(poprTop);

	int buflen = max(len * 8, 128);
	char *buffer = (char *)TempAlloc(sizeof(char), buflen);
	memcpy(buffer, src, len);
	buffer[len] = 0;

	CreateLexer(&lex, buffer, buflen, 0, 0);
	InitLexer(&lex, len+1);

	if (!TokExpr(&lex))
		return;

	InitEnvFromLexer(&env,&lex);
	env.pvarBase = poprTop;
	env.flags |= EX_KEEP_LAST;
	//TokPrint(env.pCode, env.plitBase);
	EvlExprList(&env);
}

static void FunDrop()
{
	DROPINDEX indices[MAXDIM];	// Index interator
	int		dst_drops[MAXDIM];	// Left argument
	int		dst_shape[MAXDIM];	// Shape of left argument
	int		src_shape[MAXDIM];	// Shape of right argument
	int		src_rank, dst_rank;
	int		i, j;
	int		n;
	int		copylen;
	int		src_ind, dst_ind;
	int		dst_nelem;			// # of elements in destination
	int		rhs_is_num;			// 1 if rhs is numeric, 0 if character
	double	scalar_num;			// Right argument is a scalar
	char	scalar_char;
	char	*parr;				// Generic pointer to source array

	// Left argument (destination spec)

	// Only numbers
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	if (ISARRAY(poprTop)) {			// Array (2 3↓y)
		double *pdbl;
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);
		dst_rank = NumElem(poprTop);
		if (dst_rank > MAXDIM)
			EvlError(EE_RANK);
		pdbl = VPTR(poprTop);
		for (i = 0; i < dst_rank; ++i) {
			j = (int)*pdbl++;
			if (j < -MAXIND || j > MAXIND)
				EvlError(EE_DOMAIN);
			dst_drops[i] = j;
		}
	} else {						// Scalar (2↓y)
		dst_rank = 1;
		dst_drops[0] = (int)VNUM(poprTop);
	}

	POP(poprTop);
	
	// Right argument (source)
	rhs_is_num = ISNUMBER(poprTop);

	if (ISARRAY(poprTop)) {			// Array (x↓2 3 4 or x↓'hello')
		parr = VPTR(poprTop);
		src_rank = RANK(poprTop);
		if (dst_rank > src_rank)
			EvlError(EE_DOMAIN);
		for (i = 0; i < src_rank; ++i) {
		   src_shape[i] = SHAPE(poprTop)[i];
		}
		if (dst_rank < src_rank) {	// Expand destination to rank of source
			for (i = dst_rank; i < src_rank; ++i)
				dst_drops[i] = 0;
			dst_rank = src_rank;
		}
	} else {						// Scalar (x↓3 or x↓'z')
		// Null array
		for (i = 0; i < dst_rank; ++i)
			src_shape[i] = dst_drops[i] = 0;
	}

	// At this point src_rank == dst_rank

	// Set result
	RANK(poprTop) = dst_rank;
	VOFF(poprTop) = 0;
	dst_nelem = 1;
	for (i = 0; i < dst_rank; ++i) {
		n = src_shape[i] - abs(dst_drops[i]);
		if (n < 0) n = 0;
		dst_shape[i] = SHAPE(poprTop)[i] = n;
		dst_nelem *= n;
	}

	if (dst_nelem) {
		// Create index iterator
		// There are no zero dimensions in the destination 
		copylen = CreateDropIndex(indices, dst_drops, src_shape, dst_rank, &src_ind);

		if (rhs_is_num) {
			double *psrc, *pdst;
			copylen *= sizeof(double);
			psrc = (double *)parr;
			pdst = TempAlloc(sizeof(double), dst_nelem);
			VOFF(poprTop) = WKSOFF(pdst);
			// Copy selected elements to new array
			do {
				// Copy one row at a time
				// The destination is a proper subset of the source
				// and all its rows are contiguous.
				memcpy(pdst, psrc + src_ind, copylen);
				pdst = (double *)((char *)pdst + copylen);
			} while (NextDropIndex(indices, dst_rank, &src_ind));
		} else {
			char *psrc, *pdst;
			psrc = parr;
			pdst = TempAlloc(sizeof(char), dst_nelem);
			VOFF(poprTop) = WKSOFF(pdst);
			// Copy selected elements to new array
			do {
				// Copy one row at a time
				memcpy(pdst, psrc + src_ind, copylen);
				pdst += copylen;
			} while (NextDropIndex(indices, dst_rank, &src_ind));
		}
	}
}

static void FunTake()
{
	TAKEINDEX indices[MAXDIM];	// Index interator
	int		dst_shape[MAXDIM];	// Left argument
	int		src_shape[MAXDIM];	// Right argument
	int		src_rank, dst_rank;
	int		i, j;
	int		copylen;
	int		src_ind, dst_ind;
	int		dst_nelem;			// # of elements in destination
	int		rhs_is_num;			// 1 if rhs is numeric, 0 if character
	double	scalar_num;			// Right argument is a scalar
	char	scalar_char;
	char	*parr;				// Generic pointer to source array

	// Left argument (destination spec)

	// Only numbers
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	if (ISARRAY(poprTop)) {			// Array (2 3↑y)
		double *pdbl;
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);
		dst_rank = NumElem(poprTop);
		if (dst_rank > MAXDIM)
			EvlError(EE_RANK);
		pdbl = VPTR(poprTop);
		dst_nelem = 1;
		for (i = 0; i < dst_rank; ++i) {
			j = (int)*pdbl++;
			if (j < -MAXIND || j > MAXIND)
				EvlError(EE_DOMAIN);
			dst_shape[i] = j;
			dst_nelem = (j < 0 ? -j : j) * dst_nelem;
		}
	} else {						// Scalar (2↑y)
		dst_rank = 1;
		dst_shape[0] = (int)VNUM(poprTop);
		dst_nelem = abs(dst_shape[0]);
	}

	POP(poprTop);
	
	// Right argument (source)
	rhs_is_num = ISNUMBER(poprTop);

	if (ISARRAY(poprTop)) {			// Array (x↑2 3 4 or x↑'hello')
		parr = VPTR(poprTop);
		src_rank = RANK(poprTop);
		if (dst_rank > src_rank)
			EvlError(EE_DOMAIN);
		for (i = 0; i < src_rank; ++i) {
		   src_shape[i] = SHAPE(poprTop)[i];
		}
		if (dst_rank < src_rank) {	// Expand destination to rank of source
			for (i = dst_rank; i < src_rank; ++i)
				dst_shape[i] = src_shape[i];
			dst_rank = src_rank;
		}
	} else {						// Scalar (x↑3 or x↑'z')
		if (rhs_is_num) {
			scalar_num = VNUM(poprTop);
			parr = (char *)&scalar_num;
		} else {
			scalar_char = VCHR(poprTop);
			parr = &scalar_char;
		}
		src_rank = dst_rank;		// Expand source to rank of destination
		for (i = 0; i < src_rank; ++i)
			src_shape[i] = 1;
	}

	// At this point src_rank == dst_rank

	// Set result
	RANK(poprTop) = dst_rank;
	VOFF(poprTop) = 0;
	for (i = 0; i < dst_rank; ++i)
		SHAPE(poprTop)[i] = abs(dst_shape[i]);

	if (dst_nelem) {
		// Create index iterator
		copylen = CreateTakeIndex(indices, dst_shape, src_shape, dst_rank, &dst_ind, &src_ind);

		if (rhs_is_num) {
			double *psrc, *pdst;
			copylen *= sizeof(double);
			psrc = (double *)parr;
			pdst = TempAlloc(sizeof(double), dst_nelem);
			VOFF(poprTop) = WKSOFF(pdst);
			// Initialize with zeros
			memset(pdst, 0, sizeof(double) * dst_nelem);
			// Copy selected elements to new array
			do {
				// Copy one row at a time
				memcpy(pdst + dst_ind, psrc + src_ind, copylen);
			} while (NextTakeIndex(indices, dst_rank, &dst_ind, &src_ind));
		} else {
			char *psrc, *pdst;
			psrc = parr;
			pdst = TempAlloc(1, dst_nelem);
			VOFF(poprTop) = WKSOFF(pdst);
			// Initialize with blanks
			memset(pdst, ' ', dst_nelem);
			// Copy selected elements to new array
			do {
				// Copy one row at a time
				memcpy(pdst + dst_ind, psrc + src_ind, copylen);
			} while (NextTakeIndex(indices, dst_rank, &dst_ind, &src_ind));

		}
	}
}

static int CreateTransposeIndex(int index[], int rank)
{
	// First index is [0;0;...0]
	for (int i = 0; i < rank; ++i)
		index[i] = 0;

	return 0;
}

static int NextTransposeIndex(int index[], int shape[], int size[], int rank)
{
	int ind;

	for (int j = rank - 1; j >= 0; --j) {
		if (++index[j] < shape[j])
			goto get_index;

		// Reset this level and backtrack
		index[j] = 0;
	}

	return -1;
get_index:
	ind = 0;
	for (int i = rank - 1; i >= 0; --i)
		ind += index[i] * size[i];

	return ind;
}

static void FunTranspose(void)
{
	int shape[MAXDIM];		// Shape of argument
	int index[MAXDIM];		// Argument index
	int tr_size[MAXDIM];	// Size of transpose in reverse order
	int rank;				// Rank of argument
	int	is_num;				// 1 if argument is numeric, 0 if character
	int nelem;
	int ind;

	// Leave scalars and vectors unchanged
	if ((rank = RANK(poprTop)) < 2)
		return;

	is_num = ISNUMBER(poprTop);

	nelem = 1;
	for (int i = 0; i < rank; ++i) {
		int n = SHAPE(poprTop)[i];
		shape[i] = n;
		tr_size[i] = nelem;
		nelem *= n;
	}

	// Set result

	// Reverse shape
	for (int i = 0; i < rank; ++i)
		SHAPE(poprTop)[i] = shape[rank - i - 1];

	// Nothing to transpose in null arrays
	if (!nelem)
		return;

	if (is_num) {		// Numbers
		double *pdst;
		double *psrc;
		psrc = (double *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(double), nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		ind = CreateTransposeIndex(index, rank);

		while (ind >= 0) {
			*(pdst + ind) = *psrc++;
			ind = NextTransposeIndex(index, shape, tr_size, rank);
		}
	} else {			// Characters
		char *pdst;
		char *psrc;
		psrc = (char *)VPTR(poprTop);
		pdst = TempAlloc(sizeof(char), nelem);
		VOFF(poprTop) = WKSOFF(pdst);

		ind = CreateTransposeIndex(index, rank);

		while (ind >= 0) {
			*(pdst + ind) = *psrc++;
			ind = NextTransposeIndex(index, shape, tr_size, rank);
		}
	}
}

static void FormatRow(char *pdst, double *psrc, int nc, FORMAT *pf)
{
	char buf[64];
	int n;

	for (int i = 0; i < nc; ++i, ++pf) {
		int w = pf->width;
		int p = pf->prec;
		double num = *psrc++;

		// Turn -0 into 0
		if (signbit(num) && !num) num = 0.0;

		// Add extra blank to the left as column separator
		switch (pf->fmt) {
		case FMT_INT:		// |    nnn    |
			// FMT_INT is similar to FMT_DEC, but trailing zeros and
			// a trailing decimal point are transformed into blanks.
			n = sprintf_s(buf, sizeof(buf), " %*.*f", w, p, num);
			if (strchr(buf, '.')) {
				int j;
				for (j = n - 1; j > 0 && buf[j] == '0'; --j)
					buf[j] = ' ';
				if (buf[j] == '.')
					buf[j] = ' ';
			}
			break;
		case FMT_DEC:		// |    nnn.ddd|
			n = sprintf_s(buf, sizeof(buf), " %*.*f", w, p, num);
			break;
		case FMT_EXP:		// | nnn.dddExx|
			n = sprintf_s(buf, sizeof(buf), " %*.*e", w, p, num);
			break;
		}

		// Copy result to output if it has the right length;
		// otherwise fill the output with APL_STARs.
		if (n == w + 1)	// Include leading column separator
			memcpy(pdst, buf, n);
		else {
			pdst[0] = ' ';
			memset(pdst+1, APL_STAR, w);
			n = w + 1;
		}
		pdst += n;
	}
}

static FORMAT *FormatAlloc(int nc)
{
	FORMAT *fmt_array = TempAlloc(sizeof(FORMAT), nc);
	FORMAT *pf = fmt_array;

	for (int i = 0; i < nc; ++i) {
		pf->fmt = FMT_INT;
		pf->width = 0;
		pf->prec = 0;
		++pf;
	}

	return fmt_array;
}

static void FormatUpdate(double *pdbl, int nr, int nc, FORMAT *pf)
{
	char buf[64];

	// Scan all columns
	for (int c = 0; c < nc; ++c, ++pdbl, ++pf) {
		// First determine if all the numbers in this column can be expressed
		// in %f format. If we find one that's either too big or too small,
		// we change the column format to FMT_EXP. Note that FMT_DEC is only
		// used by the dyadic ⍕ function.
		double *pd = pdbl;
		for (int r = 0; r < nr; ++r, pd += nc) {
			double num = fabs(*pd);
			if ((num < MIN_FMT_INT && num) || num > MAX_FMT_INT) {
				pf->fmt = FMT_EXP;
				break;
			}
		}

		// Now determine the appropriate width and precision for %f or %e
		pd = pdbl;
		int li = 0;	// Length of the integer part
		int lp = 0; // Length of the decimal point
		int ld = 0; // Length of the decimal part
		int le = 0;	// Length of the exponent part
		for (int r = 0; r < nr; ++r, pd += nc) {
			int n;
			char *dp;
			double num = *pd;
			// Turn -0 into 0
			if (signbit(num) && !num) num = 0.0;
			n = sprintf_s(buf, sizeof(buf), "%.*g", g_print_prec, num);
			if ((dp = strchr(buf, '.'))) {
				int d = n - (dp - buf) - 1;
				lp = 1;
				li = max(li, dp - buf);
				if (strchr(dp, 'e')) {	// -1.23e+02
					d -= 4;	// Don't count the exponent part
					le = 4;
					pf->fmt = FMT_EXP;
				}
				ld = max(ld, d);
			} else {
				li = max(li, n);
			}
		}

		pf->width = li + lp + ld + le;
		pf->prec = ld;
	}
}

static void FormatUpdateWidth(double *pdbl, int nr, int nc, FORMAT *pf)
{
	char buf[64];

	int w = 0;
	int p = pf->prec;
	int nelem = nr * nc;
	char *f = pf->fmt == FMT_DEC ? "%.*f" : "%.*e";

	// Choose the largest width for the given precision and format
	for (int i = 0; i < nelem; ++i) {
			double num = *pdbl++;
			if (signbit(num) && !num) num = 0.0;
			int n = sprintf_s(buf, sizeof(buf), f, p, num);
			w = max(w,n);
	}

	// Update width for all columns
	for (int c = 0; c < nc; ++c, ++pf)
		pf->width = w;
}

static void FormatOut()
{
	ARRAYINFO A;
	int shape[MAXDIM];
	char *pm;

	ArrayInfo(&A);
	if (!A.nelem)
		return;
	int nc = A.shape[A.rank-1];	// # of cols = # shape of last dimension
	int nr = A.nelem / nc;		// total # of rows

	FORMAT *pfmt = FormatAlloc(nc);
	FormatUpdate((double *)A.vptr, A.nelem / nc, nc, pfmt);

	// Calculate row length in characters
	int rowlen = 0;
	for (int i = 0; i < nc; ++i)
		rowlen += 1 + (pfmt + i)->width;	// Include column separator

	// Output result
	char *buf = TempAlloc(sizeof(char), rowlen + 1);
	double *psrc = (double *)A.vptr;
	int rank = A.rank;
	A.shape[rank - 1] = 1;
	for (int i = 0; i < rank; ++i)
		shape[i] = A.shape[i];

	for (int r = 0; r < nr; ++r) {
		char *pdst = buf;
		FormatRow(pdst, psrc, nc, pfmt);
		pdst[rowlen] = 0;
		psrc += nc;
		while ((pm = strchr(pdst, APL_MINUS))) {
			*pm = 0;
			// Replace APL_MINUS with '¯'
			print_line("%s¯", pdst);
			pdst = pm + 1;
		}
		print_line("%s", pdst);
		for (int i = rank - 1; i >= 0; --i) {
			if (--shape[i])
				break;
			else {
				if (r != nr - 1)
					print_line("\n");
				shape[i] = A.shape[i];
			}
		}
	}
}

static void FunFormat(void)
{
	// Characters and strings are already formatted
	if (ISCHAR(poprTop))
		return;

	ARRAYINFO A;
	ArrayInfo(&A);
	int nc = A.shape[A.rank-1];	// # of cols = # shape of last dimension
	int nr = A.nelem / nc;		// total # of rows

	FORMAT *pfmt = FormatAlloc(nc);
	FormatUpdate((double *)A.vptr, A.nelem / nc, nc, pfmt);

	// Calculate row length in characters
	int rowlen = 0;
	for (int i = 0; i < nc; ++i)
		rowlen += 1 + (pfmt + i)->width;	// Include column separator

	// Set result
	int buflen = rowlen * nr;
	char *pdst = TempAlloc(sizeof(char), buflen);
	TYPE(poprTop) = TCHR;
	RANK(poprTop) = A.rank;
	SHAPE(poprTop)[A.rank - 1] = rowlen;
	VOFF(poprTop) = WKSOFF(pdst);
	double *psrc = (double *)A.vptr;
	for (int i = 0; i < nr; ++i) {
		FormatRow(pdst, psrc, nc, pfmt);
		psrc += nc;
		pdst += rowlen;
	}
}

static void FunFormat2(void)
{
	ARRAYINFO L;
	ARRAYINFO R;
	int f, w, p;
	double *pdbl;

	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	// Characters and strings are already formatted
	if (R.type == TCHR)
		return;

	int nc = R.shape[R.rank-1];	// # of cols = # shape of last dimension
	int nr = R.nelem / nc;		// total # of rows

	// Valid lengths for L are:
	//   1   - Precision only (applied to all)
	//   2   - Width + precision (applied to all)
	//  2*nc - Width + precision (one for each column)
	FORMAT *pfmt = FormatAlloc(nc);
	FORMAT *pf = pfmt;
	pdbl = (double *)L.vptr;
	if (L.nelem == 1) {
		f = FMT_DEC;
		p = *pdbl;
		if (p < 0) {
			p = -p - 1;
			f = FMT_EXP;
		}
		for (int i = 0; i < nc; ++i, ++pf) {
			pf->fmt = f;
			pf->width = 0;
			pf->prec = p;
		}
		// Determine the width for each column
		FormatUpdateWidth((double *)R.vptr, nr, nc, pfmt);
	} else if (L.nelem == 2) {
		f = FMT_DEC;
		w = (int)pdbl[0];
		p = (int)pdbl[1];
		if (p < 0) {
			p = -p - 1;
			f = FMT_EXP;
		}
		for (int i = 0; i < nc; ++i, ++pf) {
			pf->fmt = f;
			pf->width = w;
			pf->prec = p;
		}
	} else if (L.nelem == nc * 2) {
		for (int i = 0; i < nc; ++i, ++pf) {
			f = FMT_DEC;
			w = (int)*pdbl++;
			p = (int)*pdbl++;
			if (p < 0) {
				p = -p;
				f = FMT_EXP;
			}
			pf->fmt = f;
			pf->width = w;
			pf->prec = p;
		}
	} else
		EvlError(EE_LENGTH);

	// Calculate row length in characters
	int rowlen = 0;
	for (int i = 0; i < nc; ++i)
		rowlen += 1 + (pfmt + i)->width;	// Include column separator

	// Set result
	int buflen = rowlen * nr;
	char *pdst = TempAlloc(sizeof(char), buflen);
	TYPE(poprTop) = TCHR;
	RANK(poprTop) = R.rank;
	SHAPE(poprTop)[R.rank - 1] = rowlen;
	VOFF(poprTop) = WKSOFF(pdst);
	double *psrc = (double *)R.vptr;
	for (int i = 0; i < nr; ++i) {
		FormatRow(pdst, psrc, nc, pfmt);
		psrc += nc;
		pdst += rowlen;
	}
}

static int qsort_double_up(void *base, const void *p1, const void *p2)
{
	int i1 = (int)*(double *)p1 - g_origin;
	int i2 = (int)*(double *)p2 - g_origin;
	double elem1 = *((double *)base + i1);
	double elem2 = *((double *)base + i2);

	if (elem1 < elem2) return -1;
	else if (elem1 > elem2) return 1;
	return 0;
}

static int qsort_double_down(void *base, const void *p1, const void *p2)
{
	int i1 = (int)*(double *)p1 - g_origin;
	int i2 = (int)*(double *)p2 - g_origin;
	double elem1 = *((double *)base + i1);
	double elem2 = *((double *)base + i2);

	if (elem1 < elem2) return 1;
	else if (elem1 > elem2) return -1;
	return 0;
}

static int qsort_char_up(void *base, const void *p1, const void *p2)
{
	int i1 = (int)*(double *)p1 - g_origin;
	int i2 = (int)*(double *)p2 - g_origin;
	char elem1 = *((char *)base + i1);
	char elem2 = *((char *)base + i2);

	if (elem1 < elem2) return -1;
	else if (elem1 > elem2) return 1;
	return 0;
}

static int qsort_char_down(void *base, const void *p1, const void *p2)
{
	int i1 = (int)*(double *)p1 - g_origin;
	int i2 = (int)*(double *)p2 - g_origin;
	char elem1 = *((char *)base + i1);
	char elem2 = *((char *)base + i2);

	if (elem1 < elem2) return 1;
	else if (elem1 > elem2) return -1;
	return 0;
}

static void FunGradeUpDown(int fun)
{
	ARRAYINFO V;

	if (!ISARRAY(poprTop) || RANK(poprTop) != 1)
		EvlError(EE_RANK);

	ArrayInfo(&V);

	// Result is a numeric vector (indices)
	double *base = TempAlloc(sizeof(double), V.nelem);
	double *pdst = base;
	TYPE(poprTop) = TNUM;
	VOFF(poprTop) = WKSOFF(pdst);

	// Populate vector with sequential indices
	for (int i = 0, j = g_origin; i < V.nelem; ++i, ++j)
		*pdst++ = (double)j;

	// Sort it
	if (V.type == TNUM)
		qsort_r(base, V.nelem, sizeof(double), V.vptr, fun == APL_GRADE_UP ? qsort_double_up : qsort_double_down);
	else
		qsort_r(base, V.nelem, sizeof(double), V.vptr, fun == APL_GRADE_UP ? qsort_char_up : qsort_char_down);
}

static void FunMembership()
{
	ARRAYINFO L;
	ARRAYINFO R;

	// L ∊ R

	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	if (L.type != R.type)
		EvlError(EE_DOMAIN);

	// Result is a boolean array with the same shape as L
	double *pdst = TempAlloc(sizeof(double), L.nelem);
	TYPE(poprTop) = TNUM;
	VOFF(poprTop) = WKSOFF(pdst);
	// Copy rank/shape of left argument to result
	RANK(poprTop) = L.rank;
	for (int i = 0; i < L.rank; ++i)
		SHAPE(poprTop)[i] = L.shape[i];
	
	if (L.type == TNUM) {
		double *psrL = (double *)L.vptr;
		double *psrR = (double *)R.vptr;
		for (int i = 0; i < L.nelem; ++i) {
			double num = *(psrL + i);
			double res = 0;
			for (int j = 0; j < R.nelem; ++j)
				if (*(psrR + j) == num) {
					res = 1;
					break;
				}
			*pdst++ = res;
		}
	} else {
		char *psrL = (char *)L.vptr;
		char *psrR = (char *)R.vptr;
		for (int i = 0; i < L.nelem; ++i) {
			char chr = *(psrL + i);
			double res = 0;
			for (int j = 0; j < R.nelem; ++j)
				if (*(psrR + j) == chr) {
					res = 1;
					break;
				}
			*pdst++ = res;
		}
	}
}

static void FunIota(void)
{
	double *pnew;
	double num;
	int i;
	int nElemN;

	// Argument must be numeric
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	if (ISSCALAR(poprTop))	// Either a scalar
		nElemN = (int)VNUM(poprTop);
	else {					// Or a 1-element array
		if (RANK(poprTop) != 1 || SHAPE(poprTop)[0] != 1)
			EvlError(EE_LENGTH);
		nElemN = (int)*(double *)VPTR(poprTop);
	}

	if (nElemN < 0 || nElemN > MAXIND)
		EvlError(EE_INVALID_INDEX);

	RANK(poprTop) = 1;
	SHAPE(poprTop)[0] = nElemN;

	if (!nElemN)	// Null vector
		return;

	pnew = TempAlloc(sizeof(double), nElemN);
	num = (double)g_origin;
	VOFF(poprTop) = WKSOFF(pnew);

	for (i = 1; i <= nElemN; ++i)
		*pnew++ = num++;
}

static void	FunIndexOf(void)
{
	ARRAYINFO L;
	ARRAYINFO R;

	ArrayInfo(&L);
	POP(poprTop);
	ArrayInfo(&R);

	// We don't have mixed arrays
	if (L.type != R.type)
		EvlError(EE_DOMAIN);

	// Left argument must be a scalar or a vector
	if (L.rank != 1)
		EvlError(EE_RANK);

	double *pdst = TempAlloc(sizeof(double), R.nelem);

	// Set result
	TYPE(poprTop) = TNUM;
	RANK(poprTop) = R.rank;
	VOFF(poprTop) = WKSOFF(pdst);
	
	if (R.type == TNUM) {
		for (int i = 0; i < R.nelem; ++i) {
			double numR = *((double *)R.vptr + i);
			double *psrL = (double *)L.vptr;
			double index = L.nelem + g_origin;
			for (int j = 0; j < L.nelem; ++j) {
				if (*(psrL + j) == numR) {
					index = j + g_origin;
					break;
				}
			}
			*pdst++ = index;
		}
	} else {
		for (int i = 0; i < R.nelem; ++i) {
			char chrR = *((char *)R.vptr + i);
			char *psrL = (char *)L.vptr;
			double index = L.nelem + g_origin;
			for (int j = 0; j < L.nelem; ++j) {
				if (*(psrL + j) == chrR) {
					index = j + g_origin;
					break;
				}
			}
			*pdst++ = index;
		}
	}
}

static double IdentElement(int fun)
{
	double id;

	switch (fun) {
	case APL_UP_STILE:
		id = -DBL_MAX;
		break;
	case APL_DOWN_STILE:
		id = DBL_MAX;
		break;
	case APL_EQUAL:
	case APL_TIMES:
	case APL_DIV:
	case APL_EXCL_MARK:
	case APL_STAR:
	case APL_AND:
	case APL_NOR:
	case APL_LT_OR_EQUAL:
	case APL_GT_OR_EQUAL:
		id = 1.0;
		break;
	default:
		id = 0.0;
		break;
	}

	return id;
}

static void Reduce(int fun, int axis)
{
	aplshape	shape[MAXDIM];
	int		size[MAXDIM];
	int		d, stride, nelem, n, rank, newsize;
	double	num, num2;
	double	*pnew, *pf, *pd;

	// fun/[axis]A

	if (!ISARRAY(poprTop))
		return;

	rank = RANK(poprTop);	// rank >= 1
	--rank;					// rank >= 0

	for (d = rank, nelem = 1; d >= 0; --d) {
		int sh = SHAPE(poprTop)[d];
		shape[d] = sh;
		size[d] = nelem;
		nelem *= sh;
	}

	// Remove one axis and reshape result
	RANK(poprTop) = rank;
	memcpy(&SHAPE(poprTop)[0], &shape[0], axis * sizeof(aplshape));
	memcpy(&SHAPE(poprTop)[axis], &shape[axis+1], (rank - axis) * sizeof(aplshape));

	// If shape[axis]=1 or shape[other axis]=0 (i.e. nelem = 0) there's
	// nothing to reduce. If the result is a scalar, return the identity
	// element for this function.
	if (shape[axis] == 1 || !nelem) {
		if (!rank) {
			TYPE(poprTop) = TNUM;
			VNUM(poprTop) = IdentElement(fun);
		}
		return;
	}

	pf = VPTR(poprTop);
	stride = size[axis];
	newsize = nelem / shape[axis];

	TYPE(poprTop) = TNUM;
	if (newsize == 1) {	// Result is a scalar
		pnew = (double *)&VNUM(poprTop);
	} else {			// Result is an array
		// rank > 1
		pnew = (double *)TempAlloc(sizeof(double), newsize);
		VOFF(poprTop) = WKSOFF(pnew);
	}

	do {
		// Reduce
		n = shape[axis] - 1;
		pd = pf + n * stride;
		num = *pd;
		while (n--) {
			pd -= stride;
			num = EvlDyadicScalarNumFun(fun, *pd, num);
		}

		// Store new element
		*pnew++ = num;

		// Iterate
		for (d = rank; d >= 0; --d) {
			if (d == axis)
				continue;
			if (--shape[d]) {
				pf += size[d];
				break;
			}
			shape[d] = SHAPE(poprTop)[d];
			pf -= (shape[d] - 1) * size[d];
		}
	} while (d >= 0);
}

static void Scan(int fun, int axis)
{
	int	shape[MAXDIM];	// Shape of the argument
	int size[MAXDIM];	// Sizes of axes of the argument
	int super[MAXDIM];	// Number of super-arrays for each axis
	int rank;			// Rank of the argument
	int nelem;

	// fun\[axis]A

	// If argument is not an array, leave it unchanged
	if (!ISARRAY(poprTop))
		return;

	// The only binary function that can be applied to characters is APL_EQUAL
	// and to me it doesn't make much sense to compare an accumulated value to
	// a character. Dyalog APL has this strange behavior:
	//    =\'aababc'  --> a 1 0 0 0 0
	// I think it makes more sense to raise a domain error. Besides, we don't
	// have nested arrays, which would be necessary to store mixed elements.
	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);

	rank = RANK(poprTop);

	// Fill in shape[] and size[]
	// Calculate nelem
	nelem = 1;
	for (int i = rank - 1; i >= 0; --i) {
		int n = SHAPE(poprTop)[i];
		shape[i] = n;
		size[i] = nelem;
		nelem *= n;
	}
	// If argument is an empty array, leave it unchanged
	if (!nelem)
		return;

	// Fill in super[]
	for (int i = 0, siz = 1; i < rank; ++i) {
		super[i] = siz;
		siz *= shape[i];
	}

	// Set result
	// Same rank and shape as input

	double *pdst;
	double *psrc;
	psrc = (double *)VPTR(poprTop);
	pdst = TempAlloc(sizeof(double), nelem);
	VOFF(poprTop) = WKSOFF(pdst);

	int stride = size[axis];
	int inner  = shape[axis] * size[axis];
	double accum;

	if (fun != APL_MINUS && fun != APL_DIV && fun != APL_STILE) {
		// Associative functions (scan left->right once)
		for (int i = 0; i < super[axis]; ++i) {
			for (int j = 0; j < size[axis]; ++j) {
				accum = *(psrc + j);
				*(pdst + j) = accum;
				for (int k = 1; k < shape[axis]; ++k) {
					double arg = *(psrc + j + k * stride);
					switch (fun) {
					case APL_UP_STILE:
						accum = max(accum,arg);
						break;
					case APL_DOWN_STILE:
						accum = min(accum,arg);
						break;
					case APL_PLUS:
						accum += arg;
						break;
					case APL_TIMES:
						accum *= arg;
						break;
					case APL_AND:
						accum = accum && arg;
						break;
					case APL_OR:
						accum = accum || arg;
						break;
					case APL_NAND:
						accum = !(accum && arg);
						break;
					case APL_NOR:
						accum = !(accum || arg);
						break;
					}
					*(pdst + j + k * stride) = accum;
				}
			}
			psrc += inner;
			pdst += inner;
		}
	} else {
		// Non-associative functions (scan right->left many times)
		for (int i = 0; i < super[axis]; ++i) {
			for (int j = 0; j < size[axis]; ++j) {
				psrc += inner - stride;	// last element in sub-array
				pdst += inner - stride;
				for (int k = 0; k < shape[axis] - 1; ++k) {
					accum = *(psrc + j - k * stride);
					for (int l = k + 1; l < shape[axis]; ++l) {
						double arg = *(psrc + j - l * stride);
						switch (fun) {
						case APL_MINUS:
							accum = arg - accum;
							break;
						case APL_DIV:
							if (accum == 0)
								EvlError(EE_DIVIDE_BY_ZERO);
							accum = arg / accum;
							break;
						case APL_EXCL_MARK:
							accum = Binomial(arg, accum);
							break;
						case APL_STILE:
							if (arg != 0)
								accum = fmod(accum, arg);
							else if (accum < 0)
								EvlError(EE_DOMAIN);
							break;
						case APL_STAR:
							accum = pow(arg, accum);
							break;
						case APL_LESS_THAN:
							accum = arg < accum;
							break;
						case APL_EQUAL:
							accum = arg == accum;
							break;
						case APL_GREATER_THAN:
							accum = arg > accum;
							break;
						case APL_LT_OR_EQUAL:
							accum = arg <= accum;
							break;
						case APL_NOT_EQUAL:
							accum = arg != accum;
							break;
						case APL_GT_OR_EQUAL:
							accum = arg >= accum;
							break;
						}
					}
					*(pdst + j - k * stride) = accum;
				}
				// Copy 1st element
				psrc -= inner - stride;
				pdst -= inner - stride;
				*(pdst + j) = *(psrc + j);
			}
			psrc += inner;
			pdst += inner;
		}
	}
}

static void VarGetInx(ENV *penv)
{
	DESC *pd;

	OperPush(TUND, 0);
	pd = penv->pvarBase + *penv->pCode++;
	*poprTop = *pd;
}

static void VarGetNam(ENV *penv)
{
	int len;
	char *pName;
	VNAME *pn;
	DESC *pd;

	len = *penv->pCode++;
	pName = penv->pCode;
	penv->pCode += len;
	pn = GetName(len, pName);
	if (!pn)
		EvlError(EE_UNDEFINED_VAR);
	if (!pn->odesc)
		EvlError(EE_UNDEFINED_VAR);
		
	pd = (DESC *)WKSPTR(pn->odesc);
	if (IS_VARIABLE(pd)) {			// Global variable
		OperPush(TUND, 0);
		*poprTop = *pd;
	} else if (TYPE(pd) == TFUN) {	// Niladic function
		EvlFunction((FUNCTION *)WKSPTR(VOFF(pd)));
	} else
		EvlError(EE_NOT_ATOM);
}

static void SysTimestamp(void)
{
	struct timeval tv;
	struct tm *ptm;
	double *pnum = TempAlloc(sizeof(double), 7);

	VOFF(poprTop) = WKSOFF(pnum);

	if (gettimeofday(&tv, 0)) {
		// How could this fail?
		memset(pnum, 0, 7 * sizeof(double));
		return;
	}

	// Break down
	ptm = localtime(&tv.tv_sec);
	pnum[0] = ptm->tm_year + 1900;
	pnum[1] = ptm->tm_mon + 1;
	pnum[2] = ptm->tm_mday;
	pnum[3] = ptm->tm_hour;
	pnum[4] = ptm->tm_min;
	pnum[5] = ptm->tm_sec;
	pnum[6] = tv.tv_usec;
}

static void VarGetSys(ENV *penv)
{
	char *pchr;
	double *pdbl;
	int len;

	switch (*penv->pCode++) {
	case SYS_A:		// Alphabet
		OperPush(TCHR,1);
		SHAPE(poprTop)[0] = len = 26;
		pchr = TempAlloc(sizeof(char), len);
		VOFF(poprTop) = WKSOFF(pchr);
		for (int i = 0; i < len; ++i)
			*pchr++ = 'A' + i;
		break;
	case SYS_CT:	// Compare Tolerance
		OperPush(TNUM,0);
		VNUM(poprTop) = g_comp_tol;
		break;
	case SYS_D:		// Digits
		OperPush(TCHR,1);
		SHAPE(poprTop)[0] = len = 10;
		pchr = TempAlloc(sizeof(char), len);
		VOFF(poprTop) = WKSOFF(pchr);
		for (int i = 0; i < len; ++i)
			*pchr++ = '0' + i;
		break;
	case SYS_DBG:	// Debug flags
		OperPush(TNUM,0);
		VNUM(poprTop) = g_dbg_flags;
		break;
	case SYS_IO:	// Index Origin
		OperPush(TNUM,0);
		VNUM(poprTop) = g_origin;
		break;
	case SYS_PID:	// Process id
		OperPush(TNUM,0);
		VNUM(poprTop) = getpid();
		break;
	case SYS_PP:	// Print Precision
		OperPush(TNUM,0);
		VNUM(poprTop) = g_print_prec;
		break;
	case SYS_TS:	// Timestamp
		OperPush(TNUM,1);
		SHAPE(poprTop)[0] = 7;
		SysTimestamp();
		break;
	case SYS_VER:	// Version
		OperPush(TNUM,1);
		SHAPE(poprTop)[0] = 3;
		pdbl = TempAlloc(sizeof(double), 3);
		VOFF(poprTop) = WKSOFF(pdbl);
		pdbl[0] = APL_VER_MAJOR;
		pdbl[1] = APL_VER_MINOR;
		pdbl[2] = APL_VER_PATCH;
		break;
	case SYS_WSID:	// WorkSpace ID
		OperPush(TCHR,1);
		len = strlen(pwksBase->wsid);
		SHAPE(poprTop)[0] = len;
		pchr = TempAlloc(sizeof(char), len);
		VOFF(poprTop) = WKSOFF(pchr);
		memcpy(pchr, pwksBase->wsid, len);
		break;
	}
}

static double NumValue()
{
	double num;

	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (ISSCALAR(poprTop)) {
		num = VNUM(poprTop);
	} else {
		if (RANK(poprTop) != 1 || SHAPE(poprTop)[0] != 1)
			EvlError(EE_RANK);
		num = *(double *)VPTR(poprTop);
	}

	return num;
}

static int IntValue()
{
	double num = NumValue();
	int val = (int)num;

	if ((double)val != num)	// Not an integer
		EvlError(EE_DOMAIN);

	return val;
}

static int BoolValue()
{
	int val = IntValue();

	if (val != 0 && val != 1)
		EvlError(EE_DOMAIN);

	return val;
}

static char *StrValue(int *plen)
{
	int len;
	char *ptr;

	if (!ISCHAR(poprTop))
		EvlError(EE_DOMAIN);
	if (ISSCALAR(poprTop)) {
		ptr = (char *)&poprTop->uval.xchr;
		len = 1;
	} else {
		if (RANK(poprTop) != 1)
			EvlError(EE_RANK);
		ptr = (char *)VPTR(poprTop);
		len = SHAPE(poprTop)[0];
	}

	*plen = len;
	return ptr;
}

static void VarSetSys(ENV *penv)
{
	int val, len;
	double num;
	char *ptr;

	++penv->pCode;
	switch (*penv->pCode++) {
	case SYS_CT:	// Compare Tolerance
		num = NumValue();
		g_comp_tol = num;
		break;
	case SYS_DBG:	// Debug flags
		val = IntValue();
		g_dbg_flags = val;
		break;
	case SYS_IO:	// Index Origin
		val = BoolValue();
		g_origin = val;
		break;
	case SYS_PP:	// Print Precision
		val = IntValue();
		if (val < 1 || val > 16)
			EvlError(EE_DOMAIN);
		g_print_prec = VNUM(poprTop);
		break;
	case SYS_WSID:	// WorkSpace ID
		ptr = StrValue(&len);
		if (len > WSIDSZ - 1)
			EvlError(EE_LENGTH);
		memcpy(pwksBase->wsid, ptr, len);
		pwksBase->wsid[len] = 0;
		break;
	default:
		EvlError(EE_READONLY_SYSVAR);
	}
}

static FUNCTION *VarGetFun(ENV *penv)
{
	int len;
	char *pName;
	VNAME *pn;
	DESC *pd;

	len = *penv->pCode++;
	pName = penv->pCode;
	penv->pCode += len;
	pn = GetName(len, pName);
	if (!pn)
		EvlError(EE_UNDEFINED_VAR);
	if (!pn->odesc)
		EvlError(EE_UNDEFINED_VAR);
		
	pd = (DESC *)WKSPTR(pn->odesc);
	if (!ISFUNCT(pd))
		EvlError(EE_BAD_FUNCTION);

	return (FUNCTION *)WKSPTR(VOFF(pd));
}

static int IsNullArray(DESC *pd)
{
	int i;

	if (!ISARRAY(pd))
		return 0;	// Scalar, not null array

	if (RANK(pd) == 0)
		return 1;	// Canonical null array

	// This is a null array if any of its axes has 0 elements
	for (i = 0; i < RANK(pd); ++pd)
		if (!SHAPE(pd)[i])
			return 1;

	return 0;	// All dimensions are non-null
}

static int EvlBranchLine(int previous)
{
	int line;

	// The branch line depends on the type and
	// value of the top expression

	if (TYPE(poprTop) == TNUM) {
		if (ISSCALAR(poprTop))
			line = (int)VNUM(poprTop);
		else {
			if (IsNullArray(poprTop))
				line = previous + 1;
			else {
				// Get first element of array
				line = (int)*(double *)(WKSPTR(VOFF(poprTop)));
			}
		}
	} else
		line = 0;

	return line;
}

static void EvlFunction(FUNCTION *pfun)
{
	char *base;
	ENV env;
	DESC temp;
	int i, line;

	/*
	         Function               Stack (top on the left)
	   Case  Signature              Caller           Callee
		 0   FUN ; A..Z                              Z .. A
		 1   RET ← FUN ; A..Z                        Z .. A RET
		 2   FUN ⍵ ; A..Z            ⍵               Z .. A ⍵
		 3   RET ← FUN ⍵ ; A..Z      ⍵               Z .. A ⍵ RET
		 4   ⍺ FUN ⍵ ; A .. Z        ⍺ ⍵             Z .. A ⍵ ⍺
		 5   RET ← ⍺ FUN ⍵ ; A .. Z  ⍺ ⍵             Z .. A ⍵ ⍺ RET
	*/

	InitEnvFromFunction(&env,pfun);
	// Prepare frame
	switch (pfun->nArgs * 2 + pfun->nRet) {
	case 0:		// Niladic
		break;
	case 1:		// Niladic + ret
		OperPush(TUND, 0); 
		break;
	case 2:		// Monadic
		break;
	case 3:		// Monadic + ret
		OperPush(TUND, 0);
		OperSwap();
		break;
	case 4:		// Dyadic
		OperSwap();
		break;
	case 5:		// Dyadic + ret
		temp = poprTop[1];		// Save ⍵
		poprTop[1].type = TUND;	// Ret
		poprTop[1].rank = 0;
		OperPush(TUND, 0);		// Make room for ⍵
		*poprTop = temp;		// ⍵
		break;
	}
	// Local variables
	for (i = 0; i < pfun->nLocals; ++i)
		OperPush(TUND, 0);
	env.pvarBase = poprTop;

	line = 1;
	base = env.pCode;
	do {
		// Beginning of line
		env.pCode = POINTER(base,OBJ_LINEOFF(&env,line));

		// Execute that line
		poprTop = env.pvarBase;
		EvlExprList(&env);

		// Where do we go now?
		if (*env.pCode == APL_NL) {	// Next line?
			++line;	// Proceed to the next line
		} else if (*env.pCode == APL_RIGHT_ARROW) { // Branch to other line
			// Must have a value
			VALIDATE_ARGS(&env,1);
			line = EvlBranchLine(line);
		} else
			EvlError(EE_SYNTAX_ERROR);
	} while (0 < line && line <= pfun->nLines);

	// Pop local variables and arguments
	// Leave RET on the top (if present)
	poprTop = env.pvarBase + pfun->nLocals + pfun->nArgs;
}

static void VarSetInx(ENV *penv, int dims)
{
	DESC *pd;

	/*
	   Unlike global variables, local variables always hold
	   temporary arrays, so there's no need to copy the
	   contents.
	*/
	++penv->pCode;
	pd = penv->pvarBase + *penv->pCode++;

	// Indexed assignment?
	if (!dims)	// No, just copy the descriptor
		*pd = *poprTop;
	else {		// Yes
		// We may need to create a copy of the value
		// Not easy to optimize without a reference count...
		OperPushDesc(pd);
		EvlSetIndex(dims);
	}
}

void SetName(int len, char *name, DESC *pd)
{
	VNAME *pn;

	pn = GetName(len, name);
	if (pn) {
		// Free old descriptor if any
		if (pn->odesc) {
			DESC *pdold;

			pdold = (DESC *)WKSPTR(pn->odesc);
			// Free old heap entry if any
			if (ISARRAY(pdold) || ISFUNCT(pdold))
				AplHeapFree(VOFF(pdold));
			GlobalDescFree(pdold);
		}
	} else
		pn = AddName(len, name);

	pn->odesc = WKSOFF(pd);
	pn->type  = TYPE(pd);
}

static void VarSetNam(ENV *penv, int dims)
{
	int len;
	int sizeold, sizenew;
	offset off;
	VNAME *pn;
	DESC *pd;

	++penv->pCode;
	len = *penv->pCode++;
	pn = GetName(len, penv->pCode);
	if (!pn) {
		if (dims)
			EvlError(EE_UNDEFINED_VAR);
		pn = AddName(len, penv->pCode);
	}

	penv->pCode += len;

	// If we still don't have a descriptor, get one
	if (pn->odesc)
		pd = (DESC *)WKSPTR(pn->odesc);
	else {
		if (dims)
			EvlError(EE_UNDEFINED_VAR);
		pd = GlobalDescAlloc();
		pn->odesc = WKSOFF(pd);
	}

	// Indexed assignment?
	if (dims) {
		OperPushDesc(pd);
		EvlSetIndex(dims);
		return;
	}
	// Cache value type in name table
	pn->type = TYPE(poprTop);

	// New value is a scalar
	if (ISSCALAR(poprTop)) {
		if (TYPE(poprTop) == TNUM) {
			if (ISARRAY(pd))
				AplHeapFree(VOFF(pd));
			TYPE(pd) = TNUM;
			RANK(pd) = 0;
			VNUM(pd) = VNUM(poprTop);
		} else if (TYPE(poprTop) == TCHR) {
			if (ISARRAY(pd))
				AplHeapFree(VOFF(pd));
			TYPE(pd) = TCHR;
			RANK(pd) = 0;
			VCHR(pd) = VCHR(poprTop);
		}
		return;
	}

	// New value is an array
	sizenew = NumElem(poprTop) *
		(ISNUMBER(poprTop) ? sizeof(double) : sizeof(char));

	if (ISARRAY(pd)) {	/* Old value is also an array */
		sizeold = NumElem(pd) *
			(ISNUMBER(pd) ? sizeof(double) : sizeof(char));
		if (sizeold != sizenew) {	/* If not same size then realloc */
			AplHeapFree(VOFF(pd));
			off = AplHeapAlloc(sizenew, WKSOFF(pd));
		}
		else off = VOFF(pd);
	} else
		off = AplHeapAlloc(sizenew, WKSOFF(pd));

	*pd = *poprTop;
	VOFF(pd) = off;
	memcpy((char *)WKSPTR(off), WKSPTR(VOFF(poprTop)), sizenew);
}

VNAME *GetName(int len, char *pName)
{
	offset off;
	char *pc;
	VNAME *pn;
	int i, hash;

	// Calculate name hash
	for (i = 0, pc = pName, hash = 0; i < len; ++i)
		hash += *pc++;

	off = pwksBase->hashtab[hash & (HASHSZ - 1)];

	// Search hash chain
	while (off) {
		pn = (VNAME *)WKSPTR(off);
		if (pn->len == len && !strncmp(pName, pn->name, len))
			return pn;
		off = pn->next;
	}

	return NULL;
}

VNAME *AddName(int len, char *pName)
{
	offset off;
	char *pc;
	VNAME *pn;
	int i, hash, size;

	// Allocate a new VNAME entry
	size = sizeof(VNAME) + len;

	// Name entries must be a multiple of sizeof(offset)
	size = ALIGN(size, sizeof(offset));
	if (pnamTop + size > (char *)phepBase)
		EvlError(EE_NAMETAB_FULL);
	pn = (VNAME *)pnamTop;
	pnamTop += size;

	// Fill in new VNAME entry
	pn->type = TUND;
	pn->len = len;
	pn->odesc = 0;
	memcpy(pn->name, pName, len);
	pn->name[len] = 0;

	// Calculate name hash
	for (i = 0, pc = pName, hash = 0; i < len; ++i)
		hash += *pc++;

	// Insert new VNAME entry into hash chain
	off = pwksBase->hashtab[hash & (HASHSZ - 1)];
	pwksBase->hashtab[hash & (HASHSZ - 1)] = WKSOFF(pn);
	pn->next = off;

	return pn;
}

static void OperPushDesc(DESC *pd)
{
	if (PUSH(poprTop) <= (DESC *)phepTop)
		EvlError(EE_STACK_OVERFLOW);

	*poprTop = *pd;
}

static void OperPush(int type, int rank)
{
	if (PUSH(poprTop) <= (DESC *)phepTop)
		EvlError(EE_STACK_OVERFLOW);

	TYPE(poprTop) = type;
	RANK(poprTop) = rank;
}

static void OperSwap()
{
	DESC temp;

	temp = poprTop[0];
	poprTop[0] = poprTop[1];
	poprTop[1] = temp;
}

void DescPrintln(DESC *popr)
{
	DescPrint(popr);
	print_line("\n");
}

void DescPrint(DESC *popr)
{
	int i, n, nElem, rank, width;
	int shape[MAXDIM];
	char buf[64];
	char *pch;
	double num;
	double *pdbl;

	if (RANK(poprTop) > 1)	// Give more room to higher dimensional arrays
		print_line("\n");

	switch (TYPE(popr)) {
	case TNUM:
		FormatOut();
		break;

	case TCHR:
		if (ISSCALAR(poprTop)) {
			print_line("%C", VCHR(popr));
			break;
		}
		// Copy array's shape
		rank = RANK(poprTop);
		for (i = 0; i < rank; ++i)
			shape[i] = SHAPE(popr)[i];

		// Calculate number of elements */
		nElem = NumElem(popr);

		pch = VPTR(popr);
		while (nElem--) {
			print_line("%c", *pch++);
			for (i = rank - 1; i >= 0; --i) {
				if (--shape[i])
					break;
				else {
					if (nElem)
						print_line("\n");
					shape[i] = SHAPE(popr)[i];
				}
			}
		}
		break;	
	}

	if (RANK(poprTop) > 1)	// Give more room to higher dimensional arrays
		print_line("\n");
}

static void QuoteQuadInp(void)
{
	char ioBuf[128];
	char *pnew;
	int len;

	// Null string
	OperPush(TCHR, 1);
	SHAPE(poprTop)[0] = 0;

	// Return empty vector if nothing was input
	if (fgets(ioBuf, sizeof(ioBuf), stdin) == NULL)
		return;

	if (ioBuf[0] == 0  ||  ioBuf[0] == '\n')
		return;

	// Discard newline
	len = strlen(ioBuf) - 1;
	ioBuf[len] = 0;

	// Push string
	SHAPE(poprTop)[0] = len;
	pnew = TempAlloc(sizeof(char), len);
	VOFF(poprTop) = WKSOFF(pnew);
	memcpy(pnew, ioBuf, len);
}

static void QuadInp(ENV *penv)
{
	print_line("⎕:\n");
	print_line(g_blanks);
	QuoteQuadInp();
	FunExecute(penv);
}

static void SysIdent(void)
{
	int n;
	double *pdst;

	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (ISSCALAR(poprTop))
		n = (int)VNUM(poprTop);
	else {
		if (NumElem(poprTop) != 1)
			EvlError(EE_LENGTH);
		n = (int)*(double *)(VPTR(poprTop));
	}
	if (n < 1 || n > 15)
		EvlError(EE_LENGTH);

	// Set result
	RANK(poprTop) = 2;
	SHAPE(poprTop)[0] = n;
	SHAPE(poprTop)[1] = n;
	pdst = TempAlloc(sizeof(double), n*n);
	VOFF(poprTop) = WKSOFF(pdst);

	// Zero all elements
	memset(pdst, 0, n * n * sizeof(double));
	// Set the diagonal
	for (int i = 0; i < n; ++i) {
		*pdst = 1.0;
		pdst += n + 1;
	}
}

static void SysRref(void)
{
	int nr, nc;
	double *mat;

	if (!ISNUMBER(poprTop))
		EvlError(EE_DOMAIN);
	if (!ISARRAY(poprTop) || RANK(poprTop) != 2)
		EvlError(EE_RANK);

	nr = SHAPE(poprTop)[0];
	nc = SHAPE(poprTop)[1];

	mat = TempAlloc(sizeof(double), nr * nc);
	// Copy matrix to new array
	memcpy(mat, VPTR(poprTop), nr * nc * sizeof(double));
	VOFF(poprTop) = WKSOFF(mat);

	MatRref(mat, nr, nc);
}


// Calculate number of elements in an array
static int NumElem(DESC *pv)
{
	int nElem, rank;
	aplshape *pshp;

	rank = RANK(pv) - 1;
	pshp = SHAPE(pv);

	assert(rank >= 0);

	nElem = *pshp++;
	while (rank--)
		nElem *= *pshp++;

	return nElem;
}

// Get array info
// Extends scalars to 1-element vectors
static void ArrayInfo(ARRAYINFO *pai)
{
	DESC *pd = poprTop;
	int nelem = 1;
	int rank;

	if (ISARRAY(pd)) {
		rank = RANK(pd);
		pai->vptr = VPTR(pd);
		pai->step = 1;
	} else {	// Pseudo vector
		rank = 1;
		SHAPE(pd)[0] = 1;
		if (ISNUMBER(pd)) {
			pai->xnum = VNUM(pd);
			pai->vptr = (char *)&pai->xnum;
		} else {
			pai->xchr[0] = VCHR(pd);
			pai->vptr = (char *)&pai->xchr[0];
		}
		pai->step = 0;	// Always reference the same element
	}

	// Fill in shape[] and size[]
	// Calculate nelem
	for (int i = rank - 1; i >= 0; --i) {
		int n = SHAPE(poprTop)[i];
		pai->shape[i] = n;
		pai->size[i] = nelem;
		pai->stride[i] = nelem;
		nelem *= n;
	}

	if (!ISARRAY(pd))
		pai->stride[0] = 0;

	// Fill in super[]
	for (int i = 0, size = 1; i < rank; ++i) {
		pai->super[i] = size;
		size *= pai->shape[i];
	}

	pai->type  = TYPE(pd);
	pai->rank  = rank;
	pai->nelem = nelem;
}

// Extend shape of array with new axis with length 1
static void ExtendArray(ARRAYINFO *pai, int axis)
{
	// New rank will be increased by 1
	int rank = pai->rank + 1;
	if (axis > pai->rank)
		EvlError(EE_INVALID_AXIS);
	int scalar = rank == 2 && pai->nelem == 1;

	// Open room for new axis in shape[]
	for (int i = rank - 2; i >= axis; --i)
		pai->shape[i + 1] = pai->shape[i];
	
	// New axis
	pai->shape[axis] = 1;

	// Recalculate size[], stride[]
	for (int i = rank - 1, size = 1; i >= 0; --i) {
		pai->size[i] = size;
		pai->stride[i] = scalar ? 0 : size;
		size *= pai->shape[i];
	}

	// Recalculate super[]
	for (int i = 0, siz = 1; i < rank; ++i) {
		pai->super[i] = siz;
		siz *= pai->shape[i];
	}

	pai->rank = rank;
}

// Extend shape of array with new axis with length 1
static void ExtendScalar(ARRAYINFO *psrc, ARRAYINFO *pdst, int axis)
{
	// Copy rank and shape from array
	int rank = psrc->rank;
	int nelem = 1;

	for (int i = 0; i < rank; ++i)
		pdst->shape[i] = psrc->shape[i];

	// New axis
	pdst->shape[axis] = 1;

	// Recalculate size[], stride[]
	for (int i = rank - 1; i >= 0; --i) {
		pdst->size[i] = nelem;
		pdst->stride[i] = 0;
		nelem *= pdst->shape[i];
	}

	// Recalculate super[]
	for (int i = 0, size = 1; i < rank; ++i) {
		pdst->super[i] = size;
		size *= pdst->shape[i];
	}

	pdst->rank = rank;
	pdst->nelem = nelem;
}

// Check if two arrays have the same rank and shape
static int Conformable(DESC *pv1, DESC *pv2)
{
	int rank;

	rank = RANK(pv1);

	if (RANK(pv2) != rank)
		return FALSE;

	aplshape *ps1 = &SHAPE(pv1)[0];
	aplshape *ps2 = &SHAPE(pv2)[0];

	for (int i = 0; i < rank; ++i)
		if (ps1[i] != ps2[i])
			return FALSE;

	return TRUE;
}

void *TempAlloc(int size, int nItems)
{
	char *pstk;

	// Align at the proper boundary
	pstk = (char *)ALIGN_DOWN(parrTop, size);

	size *= nItems;

	if (pstk - size <= (char *)pgblTop)
		EvlError(EE_ARRAY_OVERFLOW);

	parrTop = pstk - size;

	return (void *)parrTop;
}

DESC *GlobalDescAlloc(void)
{
	DESC *pd;

	if (pgblFree) {
		pd = pgblFree;
		pgblFree = VOFF(pd) ? (DESC *)WKSPTR(VOFF(pd)) : NULL;
		TYPE(pd) = TUND;
		return pd;
	}

	if ((char *)(pgblTop + 1) > parrTop)
		EvlError(EE_GLOBAL_DESC_FULL);

	pd = pgblTop++;
	TYPE(pd) = TUND;

	return pd;
}

void GlobalDescFree(DESC *pd)
{
	if (!pgblFree)
		VOFF(pd) = 0;
	else
		VOFF(pd) = WKSOFF(pgblFree);
	pgblFree = pd;
}

offset AplHeapAlloc(int size, offset off)
{
	HEAPCELL *pc, *pr;
	offset of;

	// The 'length' of a heap cell includes the header and
	// the data and it's always a multiple of sizeof(double).
	size += sizeof(HEAPCELL);
	size = ALIGN(size, sizeof(double));

	// See if there's a block in the free list
	// that could fulfill this request
	pr = &hepFree;
	of = pr->follow;
	while (of) {
		pc = WKSPTR(of);
		if (pc->length >= size)	// Found (first-fit)
			break;
		pr = pc;
		of = pc->follow;
	}

	if (of) {	// Use block from free list
		// If the extra space in this block is >= HEAPMINBLOCK,
		// fragment it; otherwise just use the full block and
		// waste that space.
		int extra = pc->length - size;
		if (extra >= HEAPMINBLOCK) {
			pc->length = size;
			pr->follow = WKSOFF((char *)pc + size);
			pr = (HEAPCELL *)((char *)pc + size);
			pr->length = extra;
		}
		pr->follow = pc->follow;	// Remove block from the free list
	} else {	// Get new block from the heap
		if ((char *)phepTop + size >= (char *)poprTop)
			EvlError(EE_HEAP_FULL);

		pc = phepTop;
		phepTop = (HEAPCELL *)((char *)phepTop + size);
		pc->length = size;
	}

	pc->follow = off;

	return WKSOFF((char *)pc + sizeof(HEAPCELL));
}

void AplHeapFree(offset off)
{
	HEAPCELL *pf;	// Block to free
	HEAPCELL *pr;	// Previous block in free chain
	HEAPCELL *pc;
	offset of;

	pf = (HEAPCELL *)WKSPTR(off - sizeof(HEAPCELL));
	pf->follow = 0;

	// If we're freeing the top block, simply adjust phepTop
	if ((char *)pf + pf->length == (char *)phepTop) {
		phepTop = pf;
		return;
	}

	// Try to coalesce this block with another one in the free list
	pr = &hepFree;
	of = pr->follow;
	while (of) {
		pc = WKSPTR(of);
		// Can coalesce with block before pf?
		if ((char *)pc + pc->length == (char *)pf) {
			pc->length += pf->length;
			pf = WKSPTR(pc->follow);
			// Can coalesce with the block after pf?
			if ((char *)pc + pc->length == (char *)pf) {
				pc->length += pf->length;
				pc->follow = pf->follow;
			}
			return;
		}
		// Can coalesce with block after pf?
		if ((char *)pf + pf->length == (char *)pc) {
			pf->length += pc->length;
			pr->follow = WKSOFF(pf);
			return;
		}
		// Try next block in free chain
		pr = pc;
		of = pc->follow;
	}

	// Could not coalesce; add to the free list
	pf->follow = hepFree.follow;
	hepFree.follow = WKSOFF(pf);
}

void EvlError(int errnum)
{
	PutErrorLine("\n[EvalError] %s\n", apchEvlMsg[errnum]);
	// Reset evaluation stacks
	poprTop = poprBase + 1;
	parrTop = parrBase;
	LONGJUMP();
}

/* EOF */
