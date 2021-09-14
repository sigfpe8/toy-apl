// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef __APPLE__
#include <editline/readline.h>
#endif

#include "apl.h"

/*
** Read a line from the current input stream.
** Return number of characters that were read.
**        -1 if EOF
** NUL terminates the string
** No newline character is included
*/
int FGetLine(FILE *pf, char *line, int buflen)
{
	int n;

	if (fgets(line, buflen, pf) == NULL)
		return -1;

	n = strlen(line);
	if (n > 0 && line[n - 1] == '\n')
	{
		--n;
		line[n] = '\0';
	}

	return n;
}

#ifdef	_UNIX_
int Read_line(char *prompt, char *buffer, int buflen)
{
#ifdef	__APPLE__
	char *input;
	int	len;

	if (!(input = readline(prompt)))
		return -1;	// EOF

	len = strlen(input);

	if (len) {
		// Limit input to buffer length
		if (len >= buflen) {
			len = buflen - 1;
			input[len] = 0;
		}
		
		// Copy input to buffer
		memcpy(buffer, input, len);

		add_history(input);
		free(input);
	}

	// Terminate input with 0
	buffer[len] = 0;
	return len;
#else
	print_line(prompt);
	return GetLine(buffer, buflen);
#endif
}

int GetChar(void)
{
	return getchar();
}
#endif

#ifdef	_WIN32
int GetChar(void)
{
	int ch;

	if ((ch = getchar()) != 224  &&  ch != 0)
		return ch;

	switch (getchar())
	{
		case 71: return CHAR_HOME;
		case 72: return CHAR_UP;
		case 73: return CHAR_PG_UP;
		case 75: return CHAR_LEFT;
		case 77: return CHAR_RIGHT;
		case 79: return CHAR_END;
		case 80: return CHAR_DOWN;
		case 81: return CHAR_PG_DN;
		case 82: return CHAR_INS;
		case 83: return CHAR_DEL;
	}
	return 127;
}
#endif

int print_line(char *szFmt, ...)
{
	va_list args;
	int n;

	va_start(args, szFmt);
	n = vfprintf(stdout, szFmt, args);
	va_end(args);

	return n;
}

void PutDashLine(int linelen, char *szFmt, ...)
{
	char buffer[128];
	int len, nl;
	va_list args;

	if (linelen >= sizeof(buffer))
		linelen = sizeof(buffer) - 1;

	va_start(args, szFmt);

	vsnprintf(buffer, sizeof(buffer), szFmt, args);
	len = strlen(buffer);
	if (buffer[len-1] == '\n')
	{
		// Move new-line to end
		nl = 1;
		--len;
	}
	else
		nl = 0;

	while (len < linelen)
		buffer[len++] = '-';
	buffer[len] = 0;
	printf("%s", buffer);
	if (nl)
		printf("\n");

	va_end(args);
}

void PutErrorLine(char *szFmt, ...)
{
	va_list args;

	va_start(args, szFmt);
	vfprintf(stderr, szFmt, args);
	va_end(args);
}

void PutChar(int chr)
{
	putchar(chr);
}

void PutErrorChar(int chr)
{
	putc(chr, stderr);
}

void Beep(void)
{
	putchar(7);
}

/* EOF */
