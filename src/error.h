// Released under the MIT License; see LICENSE
// Copyright (c) 2021 José Cordeiro

#ifndef _ERROR_H_
#define _ERROR_H_

// Lexical errors
//extern void LexError(int errnum);

#define	LE_NO_ERROR				0
#define	LE_BAD_TOKEN			1
#define	LE_TOO_MANY_LITERALS	2
#define	LE_BAD_NUMBER			3
#define	LE_BAD_STRING			4
#define	LE_CODE_FULL			5
#define	LE_BAD_NAME				6
#define	LE_BAD_FUNCTION_HEADER	7
#define	LE_BAD_DEL_COMMAND		8
#define LE_BAD_LABEL            9
#define LE_FUN_NOT_DEFINED      10
#define LE_FUN_ALREADY_DEFINED  11
#define LE_NAME_CONFLICT        12
#define	LE_STRING_TOO_LONG		13
#define	LE_BAD_SYSTEM_NAME		14


/* Evaluation errors */
extern void EvlError(int errnum);

#define	EE_NO_ERROR				0
#define	EE_NOT_ATOM				1
#define	EE_BAD_FUNCTION			2
#define	EE_UNMATCHED_PAR		3
#define	EE_DOMAIN				4
#define	EE_NOT_CONFORMABLE		5
#define	EE_STACK_OVERFLOW		6
#define	EE_ARRAY_OVERFLOW		7
#define	EE_DIVIDE_BY_ZERO		8
#define	EE_NAMETAB_FULL			9
#define	EE_UNDEFINED_VAR		10
#define	EE_GLOBAL_DESC_FULL		11
#define	EE_HEAP_FULL			12
#define EE_UNMATCHED_BRACKETS   13
#define EE_INVALID_INDEX        14
#define EE_NO_RETURN_VALUE      15
#define EE_SYNTAX_ERROR         16
#define EE_RANK                 17
#define	EE_LENGTH				18
#define	EE_NOT_IMPLEMENTED		19
#define	EE_INVALID_AXIS			20
#define	EE_READONLY_SYSVAR		21
#define	EE_NO_VALUE				22

/* Editor errors */
extern void EdtError(int errnum);

#define	DE_NO_ERROR				0
#define	DE_FUNCTION_TOO_BIG		1
#define	DE_BAD_LINE_NUMBER		2
#define	DE_BAD_EDIT_CMD			3
#define	DE_BAD_FUNCTION_HEADER	4

#endif /* _ERROR_H_ */

/* EOF */
