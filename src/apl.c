// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include "apl.h"
#include "error.h"
#include "token.h"

/* Workspace */
size_t	wkssz;
APLWKS  *pwksBase;

/* Name table */
size_t	namsz;
char  *pnamBase;
char  *pnamTop;

/* Heap */
size_t		hepoprsz;	// Size of heap + operand stack
HEAPCELL *	phepBase;
HEAPCELL *	phepTop;
HEAPCELL	hepFree;	// Free list

/* Operand stack */
DESC *		poprTop;

/* Global descriptors */
size_t		gblarrsz;	// Size of globals descriptors + array stack
DESC *		pdesBase;	// Also the base for the operand stack
DESC *		pgblTop;
DESC *		pgblFree;

/* Array stack */
char  *parrBase;
char  *parrTop;

/* System Global Variables */
int g_running;
int g_print_expr = 1;
int g_origin = 1;
int g_print_prec = 10;
int g_dbg_flags;
double g_comp_tol = 1e-14;
ENV *g_penv;

char *g_blanks = "      ";
char *g_blanks_del = "    ∇ ";
char *g_del = "∇";

static jmp_buf jbDirMode;
static jmp_buf jbTokExpr;

jmp_buf jbStack[NJMPBUF];
int jbSP;


static void REPL(LEXER *plex);

int main(int argc, char *argv[])
{
	LEXER lex;
	size_t rest;

	if (sizeof(DESC) != DESCSZ) {
		print_line("sizeof(DESC)=%d, expected %d\n", (int)sizeof(DESC), DESCSZ);
		exit(1);
	}

	// At this point, all sizes in KB
	wkssz = DEFWKSSZ;
	rest = wkssz - (REPLBUFSIZ/1024);

	if (wkssz <= 64)
		namsz = 2;
	else if (wkssz <= 1024)
		namsz = 8;
	else
		namsz = 16;

	rest -= namsz;
	hepoprsz = rest / 3;
	gblarrsz = wkssz - (REPLBUFSIZ/1024) - namsz - hepoprsz;

	// Convert KB sizes to Bytes
	wkssz *= 1024;
	namsz *= 1024;
	hepoprsz *= 1024;
	gblarrsz *= 1024;

	pwksBase = (APLWKS *)malloc(wkssz);
	if (pwksBase == NULL) {
		print_line("Not enough memory\n");
		exit(1);
	}

	InitWorkspace(pwksBase, 0);
	token_init();
	// The lexer buffer is at the end of the workspace and does not need to
	// be saved to disk. It needs to be inside the workspace (and cannot be,
	// for example, a local array in a function) because it contains the
	// literals table, which is accessed from the pcode via workspace offsets.
	CreateLexer(&lex, (char *)pdesBase + gblarrsz, REPLBUFSIZ, 0, 0);
	INITJUMP();

	print_line("toyAPL Version %d.%d.%d\n", APL_VER_MAJOR, APL_VER_MINOR, APL_VER_PATCH);
	print_line("Released under the MIT License; see LICENSE\n\n");

	if (argc == 1)
		REPL(&lex);
	else {
		for (int i = 1; i < argc; ++i)
			LoadFile(&lex,argv[i]);
	}

	return 0;
}

/*
   Struct APLWKS has been filled in, either by reading it in from a saved
   file or by initializing it to an empty state. Here we just instantiate
   all the run-time pointers from the workspace offsets.
*/
void GetAPLWKS(APLWKS *pws)
{
	g_origin = pws->origin;
	g_print_prec = pws->prprec;

	/* Header + name table */
	pnamBase = (char *)POINTER(pws, pws->hdrsz);
	pnamTop = (char *)POINTER(pnamBase, pws->namoff);

	/* Heap + operand stack */
	phepBase = (HEAPCELL *)POINTER(pws, pws->namsz);
	phepTop = (HEAPCELL *)POINTER(phepBase, pws->hepoff);
	hepFree.length = pws->heplen;
	hepFree.follow = pws->hepfol;

	pdesBase = (DESC *)POINTER(phepBase, pws->hepoprsz);
	poprTop = (DESC *)((char *)pdesBase - (offset)pws->oproff);

	/* Global descriptors + array stack */
	pgblTop = (DESC *)POINTER(pdesBase, pws->gbloff);

	parrBase = (char *)POINTER(pdesBase, gblarrsz);
	parrTop = parrBase - pws->arroff;
}

void SetAPLWKS(APLWKS *pws)
{
	pws->origin = g_origin;
	pws->prprec = g_print_prec;

	/* Header + name table */
	pws->namoff = OFFSET(pnamBase, pnamTop);
	pws->hepoff = OFFSET(phepBase, phepTop);
	pws->heplen = hepFree.length;
	pws->hepfol = hepFree.follow;
	pws->oproff = (char *)pdesBase - (char *)poprTop;

	/* Global descriptors + array stack */
	pws->gbloff = OFFSET(pdesBase, pgblTop);
	pws->arroff = parrBase - parrTop;
}

// Initialize the WS including user settings (origin, etc.)
void NewWorkspace(APLWKS *pws)
{
	memset(pws, 0, sizeof(APLWKS));

	pws->magic = APL_MAGIC; /* 'APL ' */

	pws->majorv = APL_VER_MAJOR;
	pws->minorv = APL_VER_MINOR;
	pws->patchv = APL_VER_PATCH;

	pws->hdrsz = sizeof(APLWKS);
	pws->wkssz = wkssz;
	pws->namsz = namsz;
	pws->hepoprsz = hepoprsz;
	pws->gblarrsz = gblarrsz;

	pws->origin = 1;
	pws->prprec = 10;

	strcpy(pwksBase->wsid, "toyAPL-WS");
}

// Initialize the WS preserving user settings (origin, etc.)
void ClearWorkspace(APLWKS *pws)
{
	char wsid[WSIDSZ];
	int origin;
	int prprec;

	// Save user settings
	origin = pws->origin;
	prprec = pws->prprec;
	strcpy_s(wsid, WSIDSZ, pws->wsid);

	// Initialize everything
	NewWorkspace(pws);

	// Restore user settings
	pws->origin = origin;
	pws->prprec = prprec;
	strcpy_s(pws->wsid, WSIDSZ, wsid);
}

void InitWorkspace(APLWKS *pws, int preserve)
{
	char wsid[WSIDSZ];
	int origin;
	int prprec;

	// Preserve user settings?
	if (preserve) {
		origin = pws->origin;
		prprec = pws->prprec;
		strcpy_s(wsid, WSIDSZ, pws->wsid);
	}

	// Initialize everything
	NewWorkspace(pws);
	GetAPLWKS(pws);

	// Restore user settings
	if (preserve) {
		pws->origin = origin;
		pws->prprec = prprec;
		strcpy_s(pws->wsid, WSIDSZ, wsid);
	}
}

static void REPL(LEXER *plex)
{
	ENV env;
	char *line = plex->psrcBase;
	int buflen = plex->buflen;
	int len;

	//print_line("pid=%d\n", getpid());

	g_running = TRUE;

	PUSHJUMP();
	SETJUMP();

	while (g_running) {
#if	0
		print_line("      ");
		if ((len = GetLine(line, REPLBUFSIZ)) < 0) {
			print_line("\n");
			break;
		}
#else
		len = Read_line(g_blanks, line, buflen);
#endif
		if (len < 0) {
			print_line("\n");
			break;	// EOF
		}
		if (line[0] == '\0')
			continue;
		if (line[0] == ')')
			SysCommand(line + 1);
		else {
			InitLexer(plex, len + 1);
			if (plex->tokTyp == APL_DEL) {
				VNAME *pn;
				// Is this an edit command:  ∇ fun [...] ?
				// Or a function definition: ∇ z ← a fun b ... ?
				NextTok(plex);
				// In any case, after the del there should be a name
				if (plex->tokTyp != APL_VARNAM)
					LexError(plex, LE_BAD_DEL_COMMAND);
				pn = GetName(plex->tokLen, plex->ptokBase);
				char *pfn = plex->ptokBase;
				NextTok(plex);
				if (plex->tokTyp == APL_LEFT_BRACKET) {	// Edit command
					if (!pn || !IS_FUNCTION(pn) || !pn->odesc)
					   LexError(plex, LE_FUN_NOT_DEFINED);
					OpenFun(plex,pn);
				} else {					// Function definition
					if (pn) {
						if (IS_FUNCTION(pn)) {	// There's a function with this name
							// If ∇ fun go edit it
							if (plex->tokTyp == APL_END)
								OpenFun(plex,pn);
							else
								LexError(plex, LE_FUN_ALREADY_DEFINED);
						} else					// There's a variable with this name
							LexError(plex, LE_NAME_CONFLICT);
					} else
						NewFun(plex, pfn);
				}
			} else {
				// Tokenize expression
				if (!TokExpr(plex))
					continue;
				// Evaluate expression
				InitEnvFromLexer(&env,plex);
				if (DEBUG_FLAG(DBG_REPL_TOKENS))
					TokPrint(env.pCode, env.plitBase);
				EvlExprList(&env);
				EvlResetStacks();
			}
		}
	}
	print_line("Good-bye!\n");
}
