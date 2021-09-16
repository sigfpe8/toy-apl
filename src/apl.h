// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#ifndef _APL_H
#define _APL_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>

// Version
#define	APL_VER_MAJOR	0
#define	APL_VER_MINOR	5
#define	APL_VER_PATCH	0

/*
 * The workspace contains no pointers, only offsets
 */

#define APL_LARGE_MM

// Small memory model
#if	defined(APL_SMALL_MM)		// 16-bit offsets, 64 KB WS, 6 dimensions
typedef uint16_t	offset;
typedef	uint8_t		apltype;
typedef	uint8_t		aplrank;
typedef	uint16_t	aplshape;
#define	MAXWKSSZ	64				// Max WS size: 64 KB
#define	DEFWKSSZ	64				// Default WS size: 64 KB
#define	BASDIM		2				// # of dims in first part of DESC
#define	MAXDIM		6				// Max # of dimensions
#define	MAXIND		65535			// Highest array index
#define	DESCSZ		16				// sizeof(DESC)

// Large memory model
#elif defined(APL_LARGE_MM)		// 32-bit offsets, 2GB WS, 14 dimensions
typedef uint32_t	offset;
typedef	uint16_t	apltype;
typedef	uint16_t	aplrank;
typedef	uint32_t	aplshape;
#define	MAXWKSSZ	(2048*1024)		// Max WS size: 2 GB
#define	DEFWKSSZ	1024			// Default WS size: 1 MB
#define	BASDIM		2				// # of dims in first part of DESC
#define	MAXDIM		14				// Max # of dimensions
#define	MAXIND		INT32_MAX		// Highest array index
#define	DESCSZ		64				// sizeof(DESC)

// Huge memory model
#elif defined(APL_HUGE_MM)		// 64-bit offsets, 16GB WS, 13 dimensions
typedef uint64_t	offset;
typedef	uint16_t	apltype;
typedef	uint16_t	aplrank;
typedef	uint32_t	aplshape;
#define	MAXWKSSZ	(16384*1024)	// Max WS size: 16 GB
#define	DEFWKSSZ	(512*1024)		// Default WS size: 512 MB
#define	BASDIM		5				// # of dims in first part of DESC
#define	MAXDIM		13				// Max # of dimensions
#define	MAXIND		INT32_MAX		// Highest array index
#define	DESCSZ		64				// sizeof(DESC)

#else
#error	"Undefined memory model"
#endif

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef	unsigned int	uint;
typedef unsigned long	ulong;

#define	OFFSET(_base,_ptr)	(offset)((char *)(_ptr)  - (char *)(_base))
#define	POINTER(_base,_off)	(void *)((char *)(_base) + (offset)(_off))

/* Some sizes */
#define	STRINGMAXSIZ	255		// String length must fit in a byte
#define	LINEMAXSIZ		255
#define	NAMEMAXSIZ		64
#define	REPLBUFSIZ		1024

/*
** The data descriptor
*/

typedef struct {
	offset	 doff;			// Data offset if array
	apltype	 type;			// Data type
	aplrank	 rank;			// Array rank
	aplshape shap[BASDIM];	// Array shape
	union {					// Scalars or continuation of shape[]
		char    xchr[8];	// Character
		double	xdbl;		// Number
		aplshape xshp[MAXDIM-BASDIM];
	} uval;
} DESC;

// APL data types
#define	TUND	0			// Undefined
#define	TINT	1			// Integer - used only to indicate a single index
#define	TNUM	2			// Floating-point number
#define	TCHR	4			// Character
#define	TBOX	8			// Box - not implemented

#define	TFUN	16			// Niladic function
#define	TFUN1	(TFUN+1)	// Monadic function
#define	TFUN2	(TFUN+2)	// Dyadic function

// Macros to access descriptor fields
#define	TYPE(p)	((p)->type)
#define	RANK(p)	((p)->rank)
#define	VNUM(p)	((p)->uval.xdbl)
#define	VCHR(p)	((p)->uval.xchr[0])
#define	VPTR(p)	((void *)((char *)pwksBase + (p)->doff))
#define	VOFF(p)	((p)->doff)
#define	SHAPE(p) ((p)->shap)

#define	ISARRAY(p)	(RANK(p) > 0)
#define	ISSCALAR(p)	(RANK(p) == 0)
#define	ISNUMBER(p)	((p)->type & (TINT | TNUM))
#define	ISCHAR(p)	((p)->type & TCHR)
#define	ISFUNCT(p)	((p)->type & TFUN)

// Macros to help conformability tests
#define	CMP_TYPES(p1,p2)	((ISARRAY(p1))*2 + (ISARRAY(p2)))
#define	CMP_SCALAR_SCALAR	0
#define	CMP_SCALAR_ARRAY	1
#define	CMP_ARRAY_SCALAR	2
#define	CMP_ARRAY_ARRAY		3

extern DESC *GlobalDescAlloc(void);
extern void GlobalDescFree(DESC *pd);

// Array organization
typedef struct {
	char *vptr;			// Pointer to elements
	int type;			// Base type (TNUM, TCHR)
	int rank;			// Array rank
	int nelem;			// # of elements
	int step;			// 0 for scalar, 1 for array
	double xnum;		// Numeric scalar
	char xchr[4];		// Character scalar
	int shape[MAXDIM];	// Elements per axis
	int size[MAXDIM];	// Inner elements
	int super[MAXDIM];	// Outer elements
	int stride[MAXDIM];	// Distance from next element
	// For regular arrays, stride == size.
	// For scalars extended to arrays, stride = 0 so that scanning
	// them always accesses the same element (the scalar).
} ARRAYINFO;

// Index iterator
typedef struct {
	int	type;		// Index type (TNUM, TARR or TNUL)
	int	index;		// Current index (0-based)
	int shape;		// Number of elements at this level
	int	size;		// Number of elements including lower levels
	double *ptr;	// Pointer to current index (array)
	double *beg;	// Pointer to first index (array)
	double *end;	// Pointer to last index+1 (array)
} INDEX;

int CreateIndex(INDEX *pi, int n);
int NextIndex(INDEX *pi, int n);

typedef struct {
	int	first;		// First index (0-based)
	int last;		// Last index (0-based)
	int	index;		// Current index (0-based)
	int shape;		// Number of elements at this level
	int size;		// Number of elements including lower levels
} INDEXRANGE;

// TakeIndex iterator (dst↑src)
typedef struct {
	INDEXRANGE	src;	// Right operand
	INDEXRANGE	dst;	// Left operand
} TAKEINDEX;

int CreateTakeIndex(TAKEINDEX *pi, int *dst_shape, int *src_shape, int rank, int *dst_index, int *src_index);
int NextTakeIndex(TAKEINDEX *pi, int rank, int *dst_index, int *src_index);

// DropIndex iterator (dst↓src)
typedef struct {
	INDEXRANGE	src;	// Right operand
} DROPINDEX;

int CreateDropIndex(DROPINDEX *pi, int *dst_drops, int *src_shape, int rank, int *psrc_ind);
int NextDropIndex(DROPINDEX *pi, int rank, int *psrc_ind);

// Axis type
#define	AXIS_DEFAULT		0
#define	AXIS_REGULAR		1
#define	AXIS_LAMINATE		2

// Validate and resolve default axis
//    _la = function which default is the last axis
#define VALIDATE_AXIS(_pd,_la)	if (ISARRAY(_pd)) {		\
			int rank = RANK(_pd);						\
			if (axis_type == AXIS_DEFAULT)				\
				axis = fun == _la ? rank - 1 : 0;		\
			else if (axis >= rank)						\
				EvlError(EE_INVALID_AXIS);				\
		} else if (axis_type != AXIS_DEFAULT)			\
			EvlError(EE_INVALID_AXIS)
/*
** The Workspace

+----------+
| Header   |<-- pwksBase
+----------+
| Name     |<-- pnamBase
| table    |<-- pnamTop
|          |
+----------+
| Heap     |<-- phepBase
|          |
|          |
+----------+
| Free1    |<-- phepTop
|          |
+----------+
| Operand  |<-- poprTop
| stack    |
|          |<-- poprBase
+----------+
| Global   |<-- pgblBase
| desc.    |
|          |<-- pgblTop
+----------+
| Free2    |
+----------+
| Array    |<-- parrTop
| stack    |
|          |<-- parrBase
+----------+

STACKS (Operand, Array, Frame)

  All stacks grow downwards.
  pxxxBase points to the first usable element (highest address).
  pxxxTop  points to the last pushed element (lowest address).
  To push an element on a stack you pre-decrement its top pointer:
	*--pxxxTop = elem;
  Top pop an element from a stack you post-increment its top pointer:
    elem = *pxxxTop++;

*/
#define	POP(top)	(++top)
#define	PUSH(top)	(--top)
#define	SECOND(top)	(top+1)
#define	ABOVE(top)	(top-1)

#define	HASHSZ	32	/* Must be a power of 2 */
#define WSIDSZ	32
#define	MAJORV	0
#define	MINORV	0
#define	LEVELV	2

typedef struct {
	uint	magic;		/* Magic number = 'APL ' */
	uint	hdrsz;		/* Header size */
	uint	namsz;		/* Name table size */
	uint	hepsz;		/* Heap size */
	uint	fr1sz;		/* Free area 1 size */
	uint	oprsz;		/* Operand stack size */
	uint	gblsz;		/* Global descriptors size */
	uint	fr2sz;		/* Free area 2 size */
	uint	arrsz;		/* Array stack size */
	uint	fr3sz;		/* Free area 3 size */
	uint	frmsz;		/* Frame stack size */
	uint	wkssz;		/* Workspace size */
	uint8_t	origin;		/* System origin (0/1) */
	uint8_t	majorv;		/* Major version # */
	uint8_t	minorv;		/* Minor version # */
	uint8_t	levelv;		/* Level version # */
	char	wsid[WSIDSZ]; /* Workspace ID (name) */
	offset	hashtab[HASHSZ];	/* Variable/Function Hash table */
} APLWKS;

#define WKSPTR(off)	POINTER(pwksBase,off)
#define	WKSOFF(ptr)	OFFSET(pwksBase,ptr)

extern APLWKS *pwksBase;
extern void InitWorkspace(int first_time);

// Name table
typedef struct {
	offset	odesc;	/* Offset to DESC currently associated */
	offset	next;	/* Offset to next NAME in colision list */
	uint8_t	len;	/* Name length */
	uint8_t	type;	/* Name type (cached from the descriptor) */
	char	name[1];/* The name itself, including null terminator */
} VNAME;

#define	IS_VARIABLE(pn)	((pn)->type < TFUN)
#define	IS_FUNCTION(pn)	((pn)->type >= TFUN)

extern size_t  namsz;
extern char  *pnamBase;
extern char  *pnamTop;

extern VNAME *AddName(int len, char *pName);
extern VNAME *GetName(int len, char *pName);
extern void SetName(int len, char *name, DESC *pd);

// Heap
typedef struct {
	offset	length;		// Total cell length (header + data)
	offset	follow;		// When in use: DESC that owns it; when free: next block in chain
#ifdef	APL_SMALL_MM
	char	pad[sizeof(double)-2*sizeof(offset)];	// Make sure data[] is aligned for doubles
#endif
} HEAPCELL;

#define	HEAPMINBLOCK	128
extern size_t	hepoprsz;	/* Size of heap + operand stack */
extern HEAPCELL	*phepBase;
extern HEAPCELL	*phepTop;
extern HEAPCELL	hepFree;

// Operand stack
extern DESC   *poprBase;
extern DESC   *poprTop;
#define	NUM_VALS(e)			((e)->pvarBase - poprTop)
#define	VALIDATE_ARGS(e,n)	if (NUM_VALS(e) < (n)) EvlError(EE_NO_VALUE)

// Global descriptors
extern size_t  gblarrsz;	// Size of globals descriptors + array stack
extern DESC   *pgblBase;
extern DESC   *pgblTop;
extern DESC   *pgblFree;

// Array stack
extern char  *parrBase;
extern char  *parrTop;

// Lexer
typedef struct {
	// Buffer for string tokens
	char		 tokStr[STRINGMAXSIZ+1];

	// Source
	char		*psrcBase;	// Beginning of source line
	char		*pChr;		// Pointer to input source
	char		*pChrBase;	// Pointer to beginning of lexChr (may occupy more than 1 byte)
	char		*psrcEnd;	// End of source line (EOL + 1)
	char		*ptokBase;	// Beginning of current token
	char		*pexprBase;	// Beginning of outermost expression
	char		*pnameBase;	// Name table (functions only)
	int			 buflen;	// Sizeof line buffer (psrcBase)
	int			 nlines;	// Capacity of linBase[(nlines+1)*2]

	// Tokens
	int			tokTyp;		// Current token
	int			tokLen;		// Length of string token
	int			tokAux;		// Auxiliary token info
	double		tokNum;		// Value of numeric token
	int			fInQuotes;	// True if inside string
	int			lexChr;		// Character after tokTyp (1-char lookahead)

	// Generated code (token string)
	char		*pCode;		// Pointer to output tokens
	char		*pobjBase;	// Base of object code

	// Literals
	double		*plitBase;	// Base of number literals
	double		*plitTop;	// Top of number literals
	int			litIndx;	// Index of next literal

	// Line offsets
	offset		*plinBase;	// Base of offsets table
} LEXER;

extern void CreateLexer(LEXER *plex, char *buffer, int buflen, int nlines, char *pnames);
extern void InitLexer(LEXER *plex, int srclen);
extern void InitLexerAux(LEXER *plex);
extern void LexError(LEXER *plex,int errnum);
extern int  TokExpr(LEXER *plex);
extern void NextChr(LEXER *plex);
extern void NextTok(LEXER *plex);

// Function header
typedef struct {
	// header - names (align) literals - line offsets - source - object

	// Sizes of some regions (in bytes)
	offset		nFunSiz;		// Total size of this structure
	offset		nHdrSiz;		// Size of header + names (offset to literal table)
	offset		nSrcSiz;		// Size of source size (including EOL)
	offset		nObjSiz;		// Size of object

	// Offsets to some regions (for convenience since they can be calculated from the sizes)
	offset		oSource;		// Offset to source (line 0)
	offset		oObject;		// Offset to object (line 1)

	// Number of items
	uint8_t		nLines;			// Number of lines (1..255)
	uint8_t		nLits;			// Number of literals
	uint8_t		nArgs;			// Number of arguments (0, 1 or 2)
	uint8_t		nLocals;		// Number of local variables (0..255)
	uint8_t		nRet;			// Return value? (0=no, 1=yes)
	uint8_t		fDirty;			// 1 if function was edited

	char		aNames[2];		// Local names table
} FUNCTION;

#define	FUN_NAM	0	//Function name (fun)
#define	FUN_RET	1	//Return value (R)
#define	FUN_ARG	2	//Function argument (A, B)
#define	FUN_LOC	3	//Local variables (C, D, ...)
#define	FUN_LAB	4	//Function labels (added later)
#define	FUN_GLB	5	//Global variables (added later)

#define	FUN_NEW		-1	// Create new function
#define	FUN_OPEN	-2	// Open existing function for editing

#define	SRC_LINEOFF(p,n)	(p)->plinBase[(n)*2]
#define	OBJ_LINEOFF(p,n)	(p)->plinBase[(n)*2+1]

// Offsets within a function are relative to the start of the header
#define	FUNOFF(pf,pt)	(offset)((char *)(pt)-(char *)(pf))
#define	FUNPTR(pf,of)	(void *)((char *)(pf)+(offset)(of))

#define	ALL_LINES	32767

extern char *FindName(char *ptable, char *pname, int len);

extern void CompileFun(FUNCTION *pfun, LEXER *plex);
extern void DumpFun(FUNCTION *pfun);
extern void EditFun(FUNCTION *pfun, LEXER *plex);
extern void FPrintFun(FILE *pf, FUNCTION *pfun);
extern void LoadFun(FILE *pf, LEXER *plex);
extern void NewFun(LEXER *plex);
extern void OpenFun(LEXER *plex, VNAME *pn);
extern void ParseHeaderFun(FUNCTION *pfun, LEXER *plex);
extern void PrintFun(FUNCTION *pfun, int nLine1, int nLine2, int fOff);
extern void SaveFun(FUNCTION *pfun, LEXER *plex);
extern void TokPrint(char *base, double *litBase);

// Evaluation environment
typedef struct {
	FUNCTION	*pFunction;	// Function being executed
	char		*pCode;		// Where the next instruction comes from
	double		*plitBase;	// Base of literals table
	offset		*plinBase;	// Base of line offsets table
	DESC		*pvarBase;	// Base of local variable descriptors
	uint32_t	flags;		// Execution flags
} ENV;

// Normally all the expressions in a diamond-separated list are evaluated,
// printed and dropped. FunExecute() sets this flag instructing EvlExprList()
// to keep the last value on the stack so that it can be used outside the
// execute.
#define	EX_KEEP_LAST	1	// Keep last value of ExprList on the stack
#define	KEEP_LAST(e)	((e)->flags & EX_KEEP_LAST)

// Printing formats for numbers
#define	FMT_INT		1		// Generic, no exp (3, ¯25, 1.345)
#define	FMT_DEC		2		// Decimal (3.00, 4.567)
#define	FMT_EXP		3		// Exponential (3.5E00, 3.45E¯12)

#define	MAX_FMT_INT	1e8
#define	MIN_FMT_INT	1e-5

// Format for a column
typedef struct {
	uint8_t		fmt;		// Format type (INT, DEC, EXP)
	uint8_t		width;		// Column width
	uint8_t		prec;		// Precision
	uint8_t		pad;
} FORMAT;

// System name indices
#define	SYS_A			1	// Alphabet
#define	SYS_CT			2	// Comparison Tolerance
#define	SYS_D			3	// Digits
#define	SYS_IDENT		4	// Identity
#define	SYS_IO			5	// Index Origin
#define	SYS_PP			6	// Print Precision
#define	SYS_RREF		7	// Reduced Row Echelon Form
#define	SYS_VER			8	// Version
#define	SYS_WSID		9	// Workspace ID
#define	SYS_TS			10	// Timestamp
#define	SYS_DBG			11	// Debug flags
#define	SYS_PID			12	// Process id

// Miscelaneous
#define	TRUE	1
#define	FALSE	0
#define	OK		1
#define	ERROR	0

#define	ALIGN(val,siz)		(((long long)(val) + (siz)-1) & -(long long)(siz))
#define	ALIGN_DOWN(val,siz)	((long long)(val) & -(long long)(siz))
#define	SIGN(val)			((val) ? ((val) < 0 ? -1 : 1) : 0)


// Global variables
extern int		g_running;
extern int		g_print_expr;
extern int		g_origin;
extern int		g_print_prec;
extern int		g_dbg_flags;
extern double	g_comp_tol;
extern char	*	g_blanks;

// Debug flags (g_dbg_flags)
#define	DEBUG_FLAG(f)		(g_dbg_flags & (f))
#define	DBG_REPL_TOKENS		1	// Display tokenization in the REPL
#define	DBG_DUMP_FUNCTION	2	// Dump function in 'save'


// Global functions
extern offset AplHeapAlloc(int size, offset off);
extern void AplHeapFree(offset off);
extern void Beep(void);
extern void DescPrint(DESC *popr);
extern void DescPrintln(DESC *popr);
extern void EmitNumber(LEXER *plex, double num);
extern void EmitTok(LEXER *plex, int tok);
extern void EvlExpr(ENV *penv);
extern void EvlExprList(ENV *penv);
extern void EvlResetStacks(void);
extern int  GetChar(void);
extern int  FGetLine(FILE *pf, char *achLine, int nLen);
#define	GetLine(buf,len)	FGetLine(stdin,buf,len)
extern void InitEnvFromLexer(ENV *penv, LEXER *plex);
extern void LoadFile(LEXER *plex, char *filename);
extern int	MatRref(double *mat, int nr, int nc);
extern void PutErrorChar(int chr);
extern void PutDashLine(int len, char *szFmt, ...);
extern void PutErrorLine(char *szFmt, ...);
extern int	print_line(char *szFmt, ...);
extern void PutChar(int chr);
extern int	Read_line(char *prompt, char *buffer, int buflen);
extern void SysCommand(char *pcmd);
extern void	*TempAlloc(int size, int nItems);


#ifdef	_UNIX_
#define	min(a,b)	((a) <= b ? (a) : (b))
#define	max(a,b)	((a) >= b ? (a) : (b))

#define	strcpy_s(_dst,_siz,_src)		strcpy(_dst,_src)
#define	sprintf_s(_buf,_siz,_fmt, ...)	sprintf(_buf,_fmt,__VA_ARGS__)
#endif

// Error recovery with longjumps
#define	NJMPBUF		4
#define	SETJUMP()	setjmp(jbStack[jbSP])
#define	LONGJUMP()	longjmp(jbStack[jbSP],1)
#define	INITJUMP()	jbSP = NJMPBUF
#define	POPJUMP()	if (++jbSP > NJMPBUF) { printf("jmp_buf underflow\n"); exit(1); }
#define PUSHJUMP()	if (--jbSP < 0) { printf("jmp_buf overflow\n"); exit(1); }

extern jmp_buf jbStack[];
extern int jbSP;

#define DEL_SYMBOL	"∇"

#endif	// _APL_H
