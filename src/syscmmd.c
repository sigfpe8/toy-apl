// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <string.h>
#include <ctype.h>

#include "apl.h"
#include "error.h"
#include "token.h"

int Clear(int argc, char **argv);
int Digits(int argc, char **argv);
int Erase(int argc, char **argv);
int Fns(int argc, char **argv);
int Heap(int argc, char **argv);
int Help(int argc, char **argv);
int Load(int argc, char **argv);
int Memory(int argc, char **argv);
int Off(int argc, char **argv);
int Origin(int argc, char **argv);
int Save(int argc, char **argv);
int Vars(int argc, char **argv);
int WsID(int argc, char **argv);

typedef struct {
	char *szName;							/* Command name	*/
	int (*pfHandler)(int argc, char **arg);	/* Handler */
	char *szHelp;							/* Help text */
} Command;

Command acmCmdTab[] =
{
	{ "clear",		Clear,		"Clear the workspace"					},
	{ "digits",		Digits,		"Set/get print precission"				},
	{ "erase",		Erase,		"Erase variable/function"				},
	{ "fns",		Fns,		"Show defined functions"				},
	{ "heap",		Heap,		"Heap statistics"						},
	{ "load",		Load,		"Load source/workspace"					},
	{ "mem",		Memory,		"Show memory usage [K|M]"				},
	{ "off",		Off,		"Exit APL"								},
	{ "origin",		Origin,		"Set/get the system origin (0/1)"		},
	{ "save",		Save,		"Save source/workspace"					},
	{ "vars",		Vars,		"Show defined variables"				},
	{ "wsid",		WsID,		"Show/change workspace ID"				},
	{ "?",			Help,		"Display help"							}
};

int  nNumCmds = sizeof(acmCmdTab) / sizeof(acmCmdTab[0]);
char achLineBuf[128];
char achLineArg[128];
int  fQuit;

#define MAXARGS	20

int  nArgc;
char *apchArgv[MAXARGS];

static int MakeArgv(void);
static Command *GetCmd(char *szName);

void SysCommand(char *pcmd)
{
	Command *pcm;
	int chr, i;

	strcpy_s((char *)achLineBuf,sizeof(achLineBuf),pcmd);

	if (MakeArgv() == ERROR)
		return;

	if ((pcm = GetCmd(apchArgv[0])) == NULL) {
		print_line("Invalid system command.\n");
		return;
	}

	if (pcm == (Command *)-1) {
		print_line("Ambiguous system command.\n");
		return;
	}

	(*pcm->pfHandler)(nArgc, apchArgv);
}

static int MakeArgv(void)
{
	char *pchO, *pchD, **ppch;

	nArgc = 0;

	pchO = achLineBuf;
	pchD = achLineArg;
	ppch = apchArgv;

	while (*pchO != '\0') {
		if (nArgc >= MAXARGS) {
			print_line("Too many arguments.\n");
			return ERROR;
		}

		while (isspace(*pchO))		/* Skip leading blanks */
			++pchO;

		if (*pchO == '\0')			/* Got to the end? */
			break;

		*ppch++ = pchD;

		if (*pchO == '"') {			/* Quoted argument */
			++pchO;
			while (*pchO != '"'  &&  *pchO != '\0')
				*pchD++ = *pchO++;
			if (*pchO == '"')
				++pchO;
		} else {					/* Ordinary argument */
			while (!isspace(*pchO)  &&  *pchO != '\0')
				*pchD++ = *pchO++;
		}

		*pchD++ = '\0';
		++nArgc;
	}

	*ppch = NULL;
	return OK;
}

static Command *GetCmd(char *szName)
{
	char *pchTab, *pchName;
	Command *pcm, *pcmFound;
	int i, nMatches;

	nMatches = 0;
	pcmFound = NULL;

	for (i = 1, pcm = acmCmdTab; i <= nNumCmds; ++i, ++pcm) {
		pchName = szName;
		pchTab  = pcm->szName;

		while (tolower(*pchName) == *pchTab++)
			if (*pchName == '\0')
				return pcm;		/* Exact match */
			else
				++pchName;

		if (*pchName == '\0') {	/* Name was a prefix */
			++nMatches;
			pcmFound = pcm;
		}
	}

	if (nMatches > 1)
		return (Command *)-1;

	return pcmFound;
}

int Clear(int argc, char *argv[])
{
	InitWorkspace(0);
	print_line("Clear WS\n");

	return OK;
}

int Digits(int argc, char **argv)
{
	int newDigits;

	if (argc == 1)
		print_line("Print precision is %d.\n", g_print_prec);
	else if (argc == 2) {
		newDigits = atoi(argv[1]);
		if (newDigits < 1 || newDigits > 16) {
			print_line(" Must be between 1 and 16.\n");
			return ERROR;
		}
		print_line("Print precision was %d\n", g_print_prec);
		g_print_prec = newDigits;
	}
	return OK;
}

int Erase(int argc, char **argv)
{
	if (argc < 2) {
		print_line(")ERASE name1 name2...\n");
		return ERROR;
	}

	for (int i = 1; i < argc; ++i) {
		VNAME *pn = GetName(strlen(argv[i]), argv[i]);
		if (pn && pn->odesc) {
			DESC *pd = WKSPTR(pn->odesc);
			if (ISARRAY(pd))
				AplHeapFree(pd->doff);
			GlobalDescFree(pd);
			pn->odesc = 0;
			pn->type = TUND;
		}
	}

	return OK;
}

int Fns(int argc, char **argv)
{
	VNAME *pn;
	int len;

	pn = (VNAME *)pnamBase;
	while ((char *)pn < pnamTop) {
		if (IS_FUNCTION(pn))
			print_line("   %s/%d\n", pn->name, pn->type - TFUN);	// Print name/arity
		len = sizeof(VNAME) + pn->len;
		len = ALIGN_UP(len, sizeof(offset));
		pn = (VNAME *)((uchar *)pn + len);
	}
	return OK;
}

int Heap(int argc, char *argv[])
{
	printf("\nHeap stats: ");
	int minl = INT32_MAX;
	int maxl = 0;
	int avgl = 0;
	int blks = 0;
	offset of = hepFree.follow;
	while (of) {
		HEAPCELL *pc = WKSPTR(of);
		++blks;
		avgl += pc->length;
		if (pc->length < minl) minl = pc->length;
		if (pc->length > maxl) maxl = pc->length;
		of = pc->follow;
	}

	if (blks) {
		printf(" %d blocks, min=%d, max=%d, avg=%d\n",
			blks, minl, maxl, avgl/blks);
	} else
		printf(" empty\n");

	return OK;
}

int Help(int argc, char *argv[])
{
	Command *pcm;

	if (argc == 1) {
		int i, nLen, nWidth = 0, w;

		print_line("Available system commands:\n\n");

		for (pcm = acmCmdTab, i = 1; i <= nNumCmds; ++i, ++pcm) {
			nLen = strlen(pcm->szName);

			if (nLen > nWidth)
				nWidth = nLen;
		}

		nWidth = (nWidth + 8) & ~7;

		for (pcm = acmCmdTab, i = 1; i <= nNumCmds; ++i, ++pcm) {
			print_line("%s", pcm->szName);
			w = strlen(pcm->szName);

			while (w < nWidth) {
				w = (w + 8) & ~7;
				put_char('\t');
			}

			print_line("%s\n", pcm->szHelp);
		}

		put_char('\n');
	} else {
		char *pch;

		pch = *++argv;
		pcm = GetCmd(pch);
		if (pcm == NULL)
			print_line("Invalid HELP command: %s\n", pch);
		else if (pcm == (Command *)-1)
			print_line("Ambiguous HELP command: %s\n", pch);
		else
			print_line("%s\t%s\n", pcm->szName, pcm->szHelp);
	}

	return OK;
}

int Load(int argc, char *argv[])
{
	int i;
	LEXER lex;

	if (argc == 1) {
		print_line("Load <file.apl>\n");
		return OK;
	}

	CreateLexer(&lex, (char *)pgblBase + gblarrsz, REPLBUFSIZ, 0, 0);

	for (i = 1; i < argc; ++i)
		LoadFile(&lex, argv[i]);

	return OK;
}

int Memory(int argc, char *argv[])
{
	size_t tsize = 0;
	size_t tused = 0;
	size_t tfree = 0;
	size_t size, used, free;
	size_t scale = 1;

	if (argc == 2) {
		if (*argv[1] == 'k' || *argv[1] == 'K') scale = 1024;
		else if (*argv[1] == 'M' || *argv[1] == 'M') scale = 1024 * 1024;
	}

	printf("Region            Size        Used        Free\n");
	printf("-----------   ---------   ---------   ---------\n");

#if	0
	// Workspace header
	size = (int)pwksBase->hdrsz;
	used = size;
	free = 0;
	tsize += size;
	tused += used;
	printf("WS header    %10d  %10d  %10d\n", size, used, free);
#endif

	// REPL input and parse buffer
	size = REPLBUFSIZ;
	used = size;
	free = 0;
	tsize += size;
	tused += used;
	printf("REPL buffer  %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	// Global name table
//	size = (int)pwksBase->namsz;
	size = namsz;
	used = (char *)pnamTop - (char *)pnamBase;
	free = (char *)phepBase - (char *)pnamTop;
	tsize += size;
	tused += used;
	tfree += free;
	printf("Name table   %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	// Global heap and operand stack grow toward each other
	size = (int)hepoprsz;
	used = (char *)phepTop  - (char *)phepBase;
	free = (char *)poprTop - (char *)phepTop;
	tsize += size;
	tused += used;
	tfree += free;
	printf("Heap         %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	// Global heap and operand stack grow toward each other
	// Same size and free space
	used = (char *)(poprBase+1) - (char *)poprTop;
	tused += used;
	printf("Oper stack   %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	// Global descriptor table and temp array stack grow toward each other
	size = (int)gblarrsz;
	used = (char *)pgblTop  - (char *)pgblBase;
	free = (char *)parrTop - (char *)pgblTop;
	tsize += size;
	tused += used;
	tfree += free;
	printf("Global desc  %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	// Global descriptor table and temp array stack grow toward each other
	// Same size and free space
	used = (char *)parrBase - (char *)parrTop;
	tused += used;
	printf("Array stack  %10d  %10d  %10d\n", (int)(size/scale), (int)(used/scale), (int)(free/scale));

	printf("              ---------   ---------   ---------\n");
	printf("Total        %10d  %10d  %10d\n", (int)(tsize/scale), (int)(tused/scale), (int)(tfree/scale));

	return OK;
}

int Off(int argc, char *argv[])
{
	g_running  = 0;
	return OK;
}

int Origin(int argc, char *argv[])
{
	int newOrigin;

	if (argc == 1)
		print_line("System ORIGIN is %d.\n", g_origin);
	else if (argc == 2) {
		newOrigin = atoi(argv[1]);
		if (newOrigin != 0 && newOrigin != 1) {
			print_line(" Invalid ORIGIN.\n");
			return ERROR;
		}
		print_line("System ORIGIN was %d\n", g_origin);
		g_origin = newOrigin;
	}
	return OK;
}

int Save(int argc, char **argv)
{
	FILE *pf;
	VNAME *pn;
	DESC *pd;
	FUNCTION *pfun;
	int i;

	if (argc < 3) {
		print_line(")SAVE fun1 fun2 ... file.apl\n");
		return ERROR;
	}

	if ((pf = fopen(argv[argc-1],"w")) == NULL) {
		print_line("Error opening %s for writing.", argv[argc-1]);
		return ERROR;
	}

	for (i = 1; i < argc - 1; ++i) {
		pn = GetName(strlen(argv[i]), argv[i]);
		if (!pn || !IS_FUNCTION(pn) || !pn->odesc) {
			print_line("Undefined function: %s\n", argv[i]);
			return ERROR;
		}
		pd = (DESC *)WKSPTR(pn->odesc);
		pfun = (FUNCTION *)WKSPTR(VOFF(pd));

		FPrintFun(pf, pfun);
	}

	fclose(pf);
	return OK;
}

int Vars(int argc, char **argv)
{
	VNAME *pn;
	int len;

	pn = (VNAME *)pnamBase;
	while ((char *)pn < pnamTop) {
		if (IS_VARIABLE(pn) && pn->type != TUND)
			print_line("   %s\n", pn->name);
		len = sizeof(VNAME) + pn->len;
		len = ALIGN_UP(len, sizeof(offset));
		pn = (VNAME *)((uchar *)pn + len);
	}
	return OK;
}

int WsID(int argc, char **argv)
{
	// Show workspace ID
	if (argc == 1) {
		print_line("%s\n", pwksBase->wsid);
		return OK;
	}

	// Change workspace ID
	if (argc == 2) {
		if (strlen(argv[1]) > WSIDSZ - 1) {
			print_line("Max length of workspace ID is %d.\n", WSIDSZ);
			return ERROR;
		}
		print_line("WAS %s\n", pwksBase->wsid);
		strcpy_s((char *)pwksBase->wsid, WSIDSZ, argv[1]);
		return OK;
	}

	print_line("Too many arguments: WSID [NAME]\n");
	return ERROR;
}

void LoadFile(LEXER *plex, char *file)
{
	ENV	env;
	char *line = plex->psrcBase;
	int	 buflen = plex->buflen;
	int save_print_expr = g_print_expr;
	FILE *pf;
	int len;

	if (!(pf = fopen(file,"r"))) {
		print_line("Could not open %s\n",file);
		return;
	}
	print_line("Loading %s\n",file);

	CreateLexer(plex, line, buflen, 0, 0);

	PUSHJUMP();
	SETJUMP();

	g_print_expr = 0;

	while ((len = FGetLine(pf, line, buflen)) >= 0) {
		if (!len) continue;

		InitLexer(plex, len + 1);
		if (plex->tokTyp == APL_RIGHT_PAREN)	// Ignore system commands
			continue;
		if (plex->tokTyp == APL_DEL)			// Function definition
			LoadFun(pf, plex);
		else {
			// Tokenize expression
			if (!TokExpr(plex))
				continue;
			// Evaluate expression
			poprTop = poprBase + 1;
			InitEnvFromLexer(&env,plex);
			EvlExprList(&env);
			EvlResetStacks();
		}
	}

	g_print_expr = save_print_expr;
	fclose(pf);
	POPJUMP();
}

void LoadFun(FILE *pf, LEXER *plex)
{
	char EditBuffer[2048];
	LEXER lex;
	FUNCTION *pfun;
	char *pfunBase, *pIns;
	int len;
	int nSrcMax;

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
	nSrcMax = pfun->nFunSiz - pfun->oSource;
	pIns = pfunBase + pfun->nSrcSiz;

	// Read lines until EOF or ∇ is found
	while ((len = FGetLine(pf, plex->psrcBase, plex->buflen)) >= 0) {
		// Ignore empty lines
		if (!len) continue;

		// Is this the end of the definition (∇)?
		InitLexer(plex, len + 1);
		if (plex->tokTyp == APL_DEL)
			break;

		// Is there room for one more line?	
		if ((pfun->nSrcSiz += len + 2) > nSrcMax) {
			// Not enough room
			pfun->nSrcSiz -= len + 2;
			EdtError(DE_FUNCTION_TOO_BIG);
		}

		// Copy new line to edit buffer, including terminator
		*pIns++ = len;
		memcpy(pIns, plex->psrcBase, len + 1);
		pIns += len + 1;
		++pfun->nLines;
	}

	// Ignore empty functions
	if (pfun->nLines) {
		// Create new lexer for the full function
		CreateLexer(&lex, pfunBase, nSrcMax, pfun->nLines, &pfun->aNames[0]);
		InitLexer(&lex, pfun->nSrcSiz);

		// Compile and save the funtion
		CompileFun(pfun, &lex);
		SaveFun(pfun, &lex);
	}
}
