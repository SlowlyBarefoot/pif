// SPDX-License-Identifier: BSD-3-Clause AND MIT

#include "core/pif_log.h"
#include "core/pif_task_manager.h"
#include "interpreter/pif_basic.h"

#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>


typedef ptrdiff_t Val;		/* SIGNED INT/POINTER */
typedef int	(*Code)();		/* BYTE-CODE */

enum {
	NAME	= 1,
	NUMBER,
	STRING,

	LP,			// '('
	RP,			// ')'
	COMMA,		// ','
	ADD,		// '+'
	SUBS,		// '-'
	MUL,		// '*'
	DIV,		// '/'
	MOD,		// '\'
	EQ,			// '='
	LT,			// '<'
	GT,			// '>'

	NE,			// '<>'
	LE,			// '<='
	GE,			// '=>'

	AND,
	OR,
	FORMAT,
	SUB,
	END,
	RETURN,
	LOCAL,
	WHILE,
	FOR,
	TO,
	IF,
	ELSE,
	THEN,
	DIM,
	UBOUND,
	BYE,
	BREAK,
	RESUME,
	EXEC
};

// Traps taken through longjmp. Every one of them ends the program: there is no prompt to drop
// back to, so a fault cannot be resumed the way the original interactive interpreter did.
enum {
	TRAP_SYNTAX	= 1,
	TRAP_FAULT	= 2,
	TRAP_BYE	= 3
};

static const char *pun = "(),+-*/\\=<>", *dub = "<><==>";

static const char* kwd[] = {
		"AND", "OR", "FORMAT", "SUB", "END",
		"RETURN", "LOCAL", "WHILE", "FOR", "TO",
		"IF", "ELSE", "THEN", "DIM", "UBOUND",
		"BYE", "BREAK", "RESUME", "EXEC", 0
};

static char lbuf[PIF_BASIC_LINE_SIZE], tokn[PIF_BASIC_SYMBOL], *lp;					/* LEXER STATE */
static int	lnum, tok, tokv, ungot;													/* LEXER STATE */
static int	(*prg[PIF_BASIC_PROGRAM])(), (**pc)(), cpc, lmap[PIF_BASIC_PROGRAM]; 	/* COMPILED PROGRAM */
static Val	stk[PIF_BASIC_STACK], *sp, *min_sp;										/* RUN-TIME STACK */
static Val	value[PIF_BASIC_VARIABLE];												/* VARIABLE VALUES */
static Val*	dim[PIF_BASIC_VARIABLE];												/* DIM MEMORY */
static char name[PIF_BASIC_VARIABLE][PIF_BASIC_SYMBOL];								/* VARIABLE NAMES */
static int	sub[PIF_BASIC_VARIABLE][PIF_BASIC_LOCAL + 2];							/* N,LOCAL VAR INDEXES */
static int	mode[PIF_BASIC_VARIABLE];												/* 0=NONE, 1=DIM, 2=SUB */
static Val	ret;																	/* FUNCTION RETURN VALUE */
static int	cstk[PIF_BASIC_STACK], *csp;											/* COMPILER STACK */
static int	cursub, temp;															/* COMPILER STATE */
static char stab[PIF_BASIC_STRING], *stabp;											/* STRING TABLE */
static jmp_buf	trap;																/* TRAP ERRORS */

#define A		sp[1]					/* LEFT OPERAND */
#define B		sp[0]					/* RIGHT OPERAND */
#define PCV		((Val)*pc++)			/* GET IMMEDIATE */
#define LOC(N)	value[sub[v][N + 2]]	/* SUBROUTINE LOCAL */
#define CSTK_EMPTY	(csp == cstk + PIF_BASIC_STACK)

static int	(*kwdhook)(char *kwd);		/* KEYWORD HOOK */


static PifBasic s_basic;


/**
 * @brief Releases the memory of every DIM array.
 */
static void freedims()
{
	int i;

	for (i = 0; i < PIF_BASIC_VARIABLE; i++) {
		if (dim[i]) {
			free(dim[i]);
			dim[i] = NULL;
		}
	}
}

/**
 * @brief Initializes interpreter runtime and compiler state. Nothing of a previous program is
 *        kept: its symbols, values, subroutines and arrays all go.
 */
static void initbasic()
{
    lnum = 0;
    ungot = 0;
    tok = 0;

	pc = prg;
	sp = stk + PIF_BASIC_STACK;
	min_sp = sp;
	csp = cstk + PIF_BASIC_STACK;
	stabp = stab;
	cpc = 0;
	cursub = -1;
	temp = 0;
	ret = 0;

	freedims();
	memset(value, 0, sizeof(value));
	memset(name, 0, sizeof(name));
	memset(sub, 0, sizeof(sub));
	memset(mode, 0, sizeof(mode));
	s_basic._variable_count = 0;
}

/**
 * @brief Reports a runtime error using the current program counter mapping and aborts execution.
 * @param msg Null-terminated runtime error message.
 */
static void err(char* msg)
{
	pifLog_Printf(LT_ERROR, "%d: %s", lmap[pc - prg - 1], msg);
	longjmp(trap, TRAP_FAULT);
}

/**
 * @brief Pushes one value onto the runtime stack, refusing to grow it past its end.
 * @param x Value to push.
 */
static void push(Val x)
{
	if (sp <= stk) err("STACK OVERFLOW");
	*--sp = x;
	if (sp < min_sp) min_sp = sp;
}

/**
 * @brief Validates a 1-based array index and returns a pointer to that element.
 * @param v Variable index of the array.
 * @param n Requested 1-based index.
 * @return Pointer to the indexed element when the index is valid.
 */
static Val* bound(int v, Val n)
{
	Val* mem = dim[v];

	if (!mem) err("ARRAY IS NOT DIMENSIONED");
	if (n < 1 || n > *mem) err("BOUNDS");
	return mem + n;
}

/**
 * @brief Reports a syntax or lexical error at the current input line and aborts parsing.
 * @param msg Null-terminated parser error message.
 */
static void bad(char* msg)
{
	pifLog_Printf(LT_ERROR, "%d: %s", lnum, msg);
	longjmp(trap, TRAP_SYNTAX);
}

/**
 * @brief Emits one opcode entry into the compiled program buffer.
 * @param opcode Function pointer for the opcode implementation.
 */
static void emit(int opcode())
{
	if (cpc >= PIF_BASIC_PROGRAM) bad("PROGRAM TOO LARGE");
	lmap[cpc] = lnum;
	prg[cpc++] = opcode;
}

/**
 * @brief Pushes one entry onto the compiler stack, refusing to grow it past its end.
 * @param x Entry to push.
 */
static void cpush(int x)
{
	if (csp <= cstk) bad("TOO MANY NESTED BLOCKS");
	*--csp = x;
}

/**
 * @brief Emits an opcode followed by an immediate operand value.
 * @param opcode Function pointer for the opcode implementation.
 * @param x Immediate operand stored after the opcode.
 */
static void inst(int opcode(), Val x)
{
	emit(opcode);
	emit((Code)x);
}

/**
 * @brief Ends the program, both at its end and at "BYE" or "BREAK".
 * @return This function does not return normally because it long-jumps.
 */
static int BYE_()
{
	longjmp(trap, TRAP_BYE);
}

/**
 * @brief Pushes an immediate numeric value onto the runtime stack.
 * @return Always returns 1 to continue execution.
 */
static int NUMBER_()
{
	push(PCV);
	return 1;
}

/**
 * @brief Pushes a variable value onto the runtime stack.
 * @return Always returns 1 to continue execution.
 */
static int LOAD_()
{
	push(value[PCV]);
	return 1;
}

/**
 * @brief Pops the runtime stack top into a variable slot.
 * @return Always returns 1 to continue execution.
 */
static int STORE_()
{
	value[PCV] = *sp++;
	return 1;
}

/**
 * @brief Prints the top stack value as an integer and pops it.
 * @return Always returns 1 to continue execution.
 */
static int ECHO_()
{
	pifLog_Printf(LT_INFO, "%d\n", (int)*sp++);
	return 1;
}

/**
 * @brief Prints a formatted string using stack arguments for % (int) and $ (string).
 * @return Always returns 1 to continue execution.
 */
static int FORMAT_()
{
	char *f;
	Val n = PCV, *ap = (sp += n) - 1;

	pifLog_PrintChar('\n');
	for (f = stab + *sp++; *f; f++)
		if (*f == '%') pifLog_Printf(LT_NONE, "%d", (int)*ap--);
		else if (*f == '$') pifLog_Print(LT_NONE, (char*)*ap--);
		else pifLog_PrintChar(*f);
	return 1;
}

/**
 * @brief Adds the right operand to the left operand on the stack.
 * @return Always returns 1 to continue execution.
 */
static int ADD_()
{
	A += B;
	sp++;
	return 1;
}

/**
 * @brief Subtracts the right operand from the left operand on the stack.
 * @return Always returns 1 to continue execution.
 */
static int SUBS_()
{
	A -= B;
	sp++;
	return 1;
}

/**
 * @brief Multiplies two top stack operands.
 * @return Always returns 1 to continue execution.
 */
static int MUL_()
{
	A *= B;
	sp++;
	return 1;
}

/**
 * @brief Divides the left operand by the right operand with zero-check protection.
 * @return Always returns 1 when successful; otherwise raises a runtime error.
 */
static int DIV_()
{
	if (!B) err("DIVISION BY ZERO");
	A /= B;
	sp++;
	return 1;
}

/**
 * @brief Computes modulus of the left operand by the right operand with zero-check protection.
 * @return Always returns 1 when successful; otherwise raises a runtime error.
 */
static int MOD_()
{
	if (!B) err("MODULUS OF ZERO");
	A %= B;
	sp++;
	return 1;
}

/**
 * @brief Compares two operands for equality and stores BASIC boolean result.
 * @return Always returns 1 to continue execution.
 */
static int EQ_()
{
	A = (A == B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Compares whether left operand is less than right operand.
 * @return Always returns 1 to continue execution.
 */
static int LT_()
{
	A = (A < B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Compares whether left operand is greater than right operand.
 * @return Always returns 1 to continue execution.
 */
static int GT_()
{
	A = (A > B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Compares two operands for inequality.
 * @return Always returns 1 to continue execution.
 */
static int NE_()
{
	A = (A != B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Compares whether left operand is less than or equal to right operand.
 * @return Always returns 1 to continue execution.
 */
static int LE_()
{
	A = (A <= B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Compares whether left operand is greater than or equal to right operand.
 * @return Always returns 1 to continue execution.
 */
static int GE_()
{
	A = (A >= B) ? -1 : 0;
	sp++;
	return 1;
}

/**
 * @brief Applies bitwise/logical AND semantics used by BASIC boolean values.
 * @return Always returns 1 to continue execution.
 */
static int AND_()
{
	A &= B;
	sp++;
	return 1;
}

/**
 * @brief Applies bitwise/logical OR semantics used by BASIC boolean values.
 * @return Always returns 1 to continue execution.
 */
static int OR_()
{
	A |= B;
	sp++;
	return 1;
}

/**
 * @brief Jumps to an absolute bytecode address stored in the immediate operand.
 * @return Always returns 1 to continue execution.
 */
static int JMP_()
{
	pc = prg + (int)*pc;
	return 1;
}

/**
 * @brief Branches when the evaluated condition is false (0).
 * @return Always returns 1 to continue execution.
 */
static int FALSE_()
{
	if (*sp++) pc++;
	else pc = prg + (int)*pc;
	return 1;
}

/**
 * @brief Evaluates FOR loop termination against the stored high bound.
 * @return Always returns 1 to continue execution.
 */
static int FOR_()
{
	if (value[PCV] >= *sp) {
		pc = prg + (int)*pc;
		sp++;
	}
	else {
		pc++;
	}
	return 1;
}

/**
 * @brief Increments FOR loop control variable.
 * @return Always returns 1 to continue execution.
 */
static int NEXT_()
{
	value[PCV]++;
	return 1;
}

/**
 * @brief Invokes a BASIC subroutine, preserving/restoring argument and local variable slots.
 * @return Always returns 1 to continue execution.
 */
static int CALL_()
{
	Val v = PCV, n = sub[v][1], x, *ap = sp;

	while (n--) {
		x = LOC(n);
		LOC(n) = *ap;
		*ap++ = x;
	}
	for (n = sub[v][1]; n < sub[v][0]; n++) push(LOC(n));
	push(pc - prg);
	pc = prg + value[v];
	return 1;
}

/**
 * @brief Returns from a BASIC subroutine and restores caller context.
 * @return Always returns 1 to continue execution.
 */
static int RETURN_()
{
	int v = PCV, n = sub[v][0];

	pc = prg + *sp++;
	while (n--) LOC(n) = *sp++;
	return 1;
}

/**
 * @brief Saves top-of-stack value as the subroutine return value.
 * @return Always returns 1 to continue execution.
 */
static int SETRET_()
{
	ret = *sp++;
	return 1;
}

/**
 * @brief Pushes the saved return value onto the runtime stack.
 * @return Always returns 1 to continue execution.
 */
static int RV_()
{
	push(ret);
	return 1;
}

/**
 * @brief Drops the specified number of temporary stack values.
 * @return Always returns 1 to continue execution.
 */
static int DROP_()
{
	sp += PCV;
	return 1;
}

/**
 * @brief Allocates array storage for a DIM variable, releasing what an earlier DIM of the same
 *        variable allocated.
 * @return Always returns 1 when allocation succeeds; otherwise raises an error.
 */
static int DIM_()
{
	int v = PCV;
	Val n = *sp++;
	Val* mem;

	if (n < 0) err("BAD DIMENSION");
	if (dim[v]) {
		free(dim[v]);
		dim[v] = NULL;
	}
	mem = calloc(n + 1, sizeof(Val));
	if (!mem) err("MEMORY IS LACK");
	mem[0] = n;
	dim[v] = mem;
	value[v] = (Val)mem;
	return 1;
}

/**
 * @brief Loads a value from a DIM array by runtime index.
 * @return Always returns 1 to continue execution.
 */
static int LOADI_()
{
	int v = PCV;

	*sp = *bound(v, *sp);
	return 1;
}

/**
 * @brief Stores a value into a DIM array by runtime index.
 * @return Always returns 1 to continue execution.
 */
static int STOREI_()
{
	int v = PCV;
	Val x = *sp++, i = *sp++;

	*bound(v, i) = x;
	return 1;
}

/**
 * @brief Pushes the upper bound (array length) of a DIM variable.
 * @return Always returns 1 to continue execution.
 */
static int UBOUND_()
{
	int v = PCV;

	if (!dim[v]) err("ARRAY IS NOT DIMENSIONED");
	push(*dim[v]);
	return 1;
}

/**
 * @brief Pops the process index and arguments of an EXEC and calls the external process.
 * @return Return value of the external process.
 */
static int exec()
{
	int p, i, param[PIF_BASIC_EXEC_SIZE];
	Val n = PCV, *ap = (sp += n) - 1;

	p = *ap-- - 1;
	if (p < 0 || p >= s_basic.__process_size) err("UNKNOWN PROCESS");
	for (i = 0; i < n - 1; i++) param[i] = *ap--;
	return (*s_basic.__p_process[p])(n - 1, param);
}

/**
 * @brief Calls an external process function without using a return value.
 * @return Always returns 1 to continue execution.
 */
static int EXEC_()
{
	exec();
	return 1;
}

/**
 * @brief Calls an external process function and pushes its return value.
 * @return Always returns 1 to continue execution.
 */
static int EXECR_()
{
	push(exec());
	return 1;
}

/**
 * @brief Finds or creates a symbol table entry for a variable name.
 * @param var Uppercase variable/subroutine identifier.
 * @return Index in the variable symbol table.
 */
static int find(char* var)
{
	int	i;

	for (i = 0; i < s_basic._variable_count && strcmp(var, name[i]); i++);
	if (i == s_basic._variable_count) {
		if (i >= PIF_BASIC_VARIABLE) bad("TOO MANY VARIABLES");
		strcpy(name[s_basic._variable_count++], var);
	}
	return i;
}

/**
 * @brief Reads the next token from the current source line.
 * @return Token type identifier, or 0 when the line ends.
 */
static int read()											/* READ TOKEN */
{
	char *p;
	const char **k, *d;

	if (ungot) {											/* UNGOT PREVIOUS */
		ungot = 0;
		return tok;
	}
	while (isspace((unsigned char)*lp)) lp++;				/* SKIP SPACE */
	if (!*lp || *lp == '#') return tok = 0;					/* END OF LINE */
	if (isdigit((unsigned char)*lp)) {						/* NUMBER */
		tokv = strtol(lp, &lp, 0);
		return tok = NUMBER;
	}
	if ((p = strchr(pun, *lp)) && lp++) { 					/* PUNCTUATION */
		for (d = dub; *d && strncmp(d, lp - 1, 2); d += 2);
		if (!*d) return tok = (p - pun) + LP;
		lp++;
		return tok = (d - dub) / 2 + NE;
	}
	else if (isalpha((unsigned char)*lp)) {					/* IDENTIFIER */
		for (p = tokn; isalnum((unsigned char)*lp); ) {
			if (p >= tokn + PIF_BASIC_SYMBOL - 1) bad("NAME TOO LONG");
			*p++ = toupper((unsigned char)*lp++);
		}
		for (*p = 0, k = kwd; *k && strcmp(tokn, *k); k++);
		if (*k) return tok = (k - kwd) + AND;
		tokv = find(tokn);
		return tok = NAME;
	}
	else if (*lp=='"' && lp++) {							/* STRING */
		for (p = stabp; *lp && *lp != '"'; ) {
			if (stabp >= stab + PIF_BASIC_STRING - 1) bad("STRING TABLE FULL");
			*stabp++ = *lp++;
		}
		if (*lp != '"') bad("UNTERMINATED STRING");
		*stabp++ = 0;
		lp++;
		tokv = p - stab;
		return tok = STRING;
	}
	else {
		bad("BAD TOKEN");
		return 0;
	}
}

/**
 * @brief Checks whether the next token matches the requested type.
 * @param type Expected token type.
 * @return Non-zero when the token matches; otherwise zero and token is left ungot.
 */
static int want(int type)
{
	return !(ungot = read() != type);
}

/**
 * @brief Enforces that the next token matches a required type.
 * @param type Required token type.
 */
static void need(int type)
{
	if (!want(type)) bad("SYNTAX ERROR");
}

/**
 * @brief Parses a full expression including logical AND/OR precedence.
 * @return Always returns 0 after emitting bytecode for the expression.
 */
static int expr();

#define LIST(BODY) if (!want(0)) do { BODY; } while (want(COMMA))

/**
 * @brief Parses the arguments of an EXEC and emits the opcode that runs it.
 * @param opcode EXEC_ for a statement, EXECR_ for an expression.
 */
static void execargs(int opcode())
{
	int n = 0;

	LIST(expr(); n++);
	if (n < 1) bad("NO PROCESS");
	if (n - 1 > PIF_BASIC_EXEC_SIZE) bad("TOO MANY ARGS");
	inst(opcode, n);
}

/**
 * @brief Parses and emits bytecode for base-level expressions (literals, variables, calls, grouping).
 */
static void base()					/* BASIC EXPRESSION */
{
	int neg = want(SUBS) ? (inst(NUMBER_, 0), 1) : 0;

	if (want(NUMBER)) inst(NUMBER_, tokv);
	else if (want(STRING)) inst(NUMBER_, (Val)(stab + tokv));
	else if (want(NAME)) {
		int var = tokv;
		if (want(LP)) {
			if (mode[var] == 1) {	/* DIM */
				expr();
				need(RP);
				inst(LOADI_, var);
			}
			else {
				int n = 0;
				LIST(if (tok == RP) break; expr(); n++);
				need(RP);
				if (mode[var] != 2 || n != sub[var][1])
					bad("BAD SUB/ARG COUNT");
				inst(CALL_, var);
				emit(RV_);
			}
		}
		else inst(LOAD_, var);
	}
	else if (want(LP)) {
		expr();
		need(RP);
	}
	else if (want(UBOUND)) {
		need(LP);
		need(NAME);
		if (mode[tokv] != 1) bad("NOT AN ARRAY");
		need(RP);
		inst(UBOUND_, tokv);
	}
	else if (want(EXEC)) execargs(EXECR_);
	else bad("BAD EXPRESSION");
	if (neg) emit(SUBS_);			/* NEGATE */
}

static int (*bin[])() = { ADD_, SUBS_, MUL_, DIV_, MOD_, EQ_, LT_, GT_, NE_, LE_, GE_, AND_, OR_};

/**
 * @brief Parses multiplicative-precedence expression terms.
 * @return Always returns 0 after emitting bytecode for the parsed term.
 */
static int factor()
{
	int (*o)();

	base();
	while (want(0), MUL <= tok && tok <= MOD) {
		o = bin[tok - ADD];
		read();
		base();
		emit(o);
	}
	return 0;
}

/**
 * @brief Parses additive-precedence expression terms.
 * @return Always returns 0 after emitting bytecode for the parsed term.
 */
static int addition()
{
	int (*o)();

	factor();
	while (want(0), ADD <= tok && tok <= SUBS) {
		o = bin[tok - ADD];
		read();
		factor();
		emit(o);
	}
	return 0;
}

/**
 * @brief Parses relational operators (=, <, >, <=, >=, <>).
 * @return Always returns 0 after emitting bytecode for the parsed term.
 */
static int relation()
{
	int (*o)();

	addition();
	while (want(0), EQ <= tok && tok <= GE) {
		o = bin[tok - ADD];
		read();
		addition();
		emit(o);
	}
	return 0;
}

/**
 * @brief Parses a full expression including logical AND/OR precedence.
 * @return Always returns 0 after emitting bytecode for the expression.
 */
static int expr()
{
	int (*o)();

	relation();
	while (want(0), AND <= tok && tok <= OR) {
		o = bin[tok - ADD];
		read();
		relation();
		emit(o);
	}
	return 0;
}

/**
 * @brief Parses one BASIC statement and emits the corresponding bytecode.
 */
static void stmt()	/* STATEMENT */
{
	int	n, var, *saved;
	char *f;

	switch (read()) {
	case FORMAT:
		need(STRING);
		var = tokv;
		inst(NUMBER_, var);
		n = 0;
		if (want(COMMA)) LIST(expr(); n++);
		for (f = stab + var, var = 0; *f; f++)				/* EVERY "%" AND "$" TAKES AN ARGUMENT */
			if (*f == '%' || *f == '$') var++;
		if (var > n) bad("TOO FEW FORMAT ARGS");
		inst(FORMAT_, n);
		break;

	case SUB:												/* CSTK: {SUB,INDEX,JMP} */
		if (!CSTK_EMPTY) bad("SUB MUST BE AT TOP LEVEL");
		need(NAME);											/* SUB NAME */
		var = tokv;
		if (mode[var]) bad("NAME ALREADY USED");
		mode[cursub = var] = 2;
		n = 0; 												/* PARAMS */
		LIST(need(NAME); if (n >= PIF_BASIC_LOCAL) bad("TOO MANY PARAMS"); sub[var][n++ + 2] = tokv);
		cpush(cpc + 1);										/* JUMP OVER CODE */
		inst(JMP_, 0);
		sub[var][0] = sub[var][1] = n;						/* LOCAL=PARAM COUNT */
		value[var] = cpc;									/* ADDRESS */
		cpush(var);											/* FOR "END" CLAUSE */
		cpush(SUB);
		break;

	case LOCAL:
		if (cursub < 0) bad("LOCAL OUTSIDE SUB");
		LIST(need(NAME); if (sub[cursub][0] >= PIF_BASIC_LOCAL) bad("TOO MANY LOCALS");
			sub[cursub][sub[cursub][0]++ + 2] = tokv);
		break;

	case RETURN:
		if (cursub < 0) bad("RETURN OUTSIDE SUB");
		if (temp) inst(DROP_, temp);
		if (!want(0)) {
			expr();
			emit(SETRET_);
		}
		inst(RETURN_, cursub);
		break;

	case WHILE:												/* CSTK: {WHILE,TEST-FALSE,TOP} */
		cpush(cpc);
		expr();
		cpush(cpc + 1);
		cpush(WHILE);
		inst(FALSE_, 0);
		break;

	case FOR:												/* CSTK: {FOR,TEST-FALSE,I,TOP}; STK:{HI} */
		need(NAME);
		var = tokv;
		temp++;
		need(EQ);
		expr();
		inst(STORE_, var);
		need(TO);
		expr();
		cpush(cpc);
		inst(FOR_, var);
		emit(0);
		cpush(var);
		cpush(cpc - 1);
		cpush(FOR);
		break;

	case IF:												/* CSTK: {IF,N,ENDS...,TEST-FALSE} */
		expr();
		inst(FALSE_, 0);
		cpush(cpc - 1);
		if (want(THEN)) {
			// Only a statement that neither opens nor closes a block may follow THEN, because
			// the test-false fixup on top of the compiler stack is patched right after it.
			n = read();
			ungot = 1;
			if (n == END || n == ELSE) bad("BLOCK STATEMENT AFTER THEN");
			saved = csp;
			stmt();
			if (csp != saved) bad("BLOCK STATEMENT AFTER THEN");
			prg[*csp++] = (Code)cpc;
		}
		else {
			cpush(0);
			cpush(IF);
		}
		break;

	case ELSE:
		if (CSTK_EMPTY || *csp != IF) bad("ELSE WITHOUT IF");
		n = csp[1] + 1;
		if (!csp[1 + n]) bad("ELSE AFTER ELSE");			/* NO TEST LEFT TO PATCH */
		inst(JMP_, 0);										/* JUMP OVER "ELSE" */
		cpush(IF); 											/* ADD A FIXUP */
		csp[1] = n;
		csp[2] = cpc - 1;
		prg[csp[2 + n]] = (Code)cpc;						/* PATCH "ELSE" */
		csp[2 + n] = !want(IF) ? 0 : (expr(), inst(FALSE_, 0), cpc - 1);	/* "ELSE IF" */
		break;

	case END:
		if (CSTK_EMPTY) bad("END WITHOUT BLOCK");
		need(*csp++);										/* MATCH BLOCK */
		if (csp[-1] == SUB) {
			inst(RETURN_, *csp++);
			prg[*csp++] = (Code)cpc;						/* PATCH JUMP */
			cursub = -1;
		}
		else if (csp[-1] == WHILE) {
			prg[*csp++] = (Code)(cpc + 2);					/* PATCH TEST */
			inst(JMP_, *csp++);								/* LOOP TO TEST */
		}
		else if (csp[-1] == FOR) {
			prg[*csp++] = (Code)(cpc + 4);					/* PATCH TEST */
			inst(NEXT_, *csp++);							/* INCREMENT */
			inst(JMP_, *csp++);								/* LOOP TO TEST */
			temp--;											/* ONE LESS TEMP */
		}
		else if (csp[-1] == IF) {
			for (n = *csp++; n--; )							/* PATCH BLOCK ENDS */
				prg[*csp++] = (Code)cpc;
			n = *csp++;
			if (n) prg[n] = (Code)cpc;						/* PATCH "ELSE" */
		}
		break;

	case NAME:
		var = tokv;
		if (want(EQ)) {
			expr();
			inst(STORE_, var);
		}
		else if (want(LP)) {
			expr();
			need(RP);
			need(EQ);
			expr();
			inst(STOREI_, var);
		}
		else if (!kwdhook || !kwdhook(tokn)) {
			int n = 0;
			LIST(expr(); n++);
			if (mode[var] != 2 || n != sub[var][1])
				bad("BAD SUB/ARG COUNT");
			inst(CALL_, var);
		}
		break;

	case DIM:
		need(NAME);											/* SET VAR MODE TO DIM */
		var = tokv;
		if (mode[var] == 2) bad("NAME ALREADY USED");
		mode[var] = 1;
		need(LP);
		expr();
		need(RP);
		inst(DIM_, var);
		break;

	case RESUME:
		bad("RESUME IS NOT SUPPORTED");
		break;

	case BREAK:
	case BYE:
		emit(BYE_);
		break;

	case GT:
		expr();
		emit(ECHO_);
		break;

	case EXEC:
		execargs(EXEC_);
		break;

	default:
		if (tok) bad("BAD STATEMENT");
		break;
	}
	if (!want(0)) bad("TOKENS AFTER STATEMENT");
}

#define USE_KWDHOOK	0

#if USE_KWDHOOK

/**
 * @brief Prints a string value from the runtime stack (debug keyword hook helper).
 * @return Always returns 1 to continue execution.
 */
static int PRINTS_()
{
	pifLog_Print(LT_NONE, (char*)*sp++);
	return 1;
}

/**
 * @brief Handles optional custom keywords enabled by USE_KWDHOOK.
 * @param msg Keyword text in uppercase form.
 * @return Non-zero when the keyword is handled; otherwise zero.
 */
static int kwdhook_(char *msg)
{
	if (!strcmp(msg, "PRINTS")) {
		expr();
		emit(PRINTS_);
	}
	else return 0;
	return 1;
}

#endif

/**
 * @brief Records the outcome of a program and reports it.
 * @param p_owner Pointer to the owner instance.
 * @param result Whether the program ran to its end without an error.
 */
static void _finish(PifBasic* p_owner, BOOL result)
{
	freedims();

	p_owner->_result = result;
	p_owner->_program_size = cpc;
	p_owner->_string_count = stabp - stab;
	p_owner->_stack_count = (int)(stk + PIF_BASIC_STACK - min_sp);
	p_owner->_process_time = pif_cumulative_timer1ms - p_owner->__start_time - p_owner->_parsing_time;
	p_owner->__state = BIS_IDLE;

	if (p_owner->__evt_result) (*p_owner->__evt_result)(p_owner);
}

/**
 * @brief Reads the next source line into the lexer buffer and advances past it. A line ends at
 *        "\n", "\r", "\r\n" or "\n\r".
 * @param p_owner Pointer to the owner instance.
 */
static void _readLine(PifBasic* p_owner)
{
	char *p_line = p_owner->__p_current;
	int n = strcspn(p_line, "\r\n");

	if (n >= PIF_BASIC_LINE_SIZE) bad("LINE TOO LONG");
	memcpy(lbuf, p_line, n);
	lbuf[n] = 0;
	lp = lbuf;

	if (!p_line[n]) {
		p_owner->__p_current = NULL;
	}
	else {
		if (p_line[n + 1] == (p_line[n] == '\r' ? '\n' : '\r')) n++;
		p_owner->__p_current = p_line + n + 1;
	}
}

/**
 * @brief Task callback that parses, compiles, and executes a BASIC program. One release does at
 *        most one source line or PIF_BASIC_OPCODE opcodes and then asks to be released again, so
 *        that a long program does not hold the CPU. __state is where it left off; every piece of
 *        interpreter state other than that already lives outside this call.
 * @param p_task Task context containing the owner instance.
 * @return 1 to be released again, 0 when the program is over.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifBasic* p_owner = (PifBasic*)p_task->_p_client;
	int code;
	int cnt;

	if (p_owner->__state == BIS_IDLE) return 0;

	if (p_owner->__state == BIS_START) {
		p_owner->__p_current = p_owner->__p_program;
		p_owner->__start_time = pif_cumulative_timer1ms;
		p_owner->_parsing_time = 0UL;

		initbasic();

#if USE_KWDHOOK
		kwdhook = kwdhook_;
#endif
		p_owner->__state = BIS_LINE;
	}

	// Set up again on every release, because a longjmp can only land in a call that is still on the
	// stack. Every trap ends the program.
	code = setjmp(trap);						/* RETURN ON ERROR */
	if (code) {
		_finish(p_owner, code == TRAP_BYE);
		return 0;
	}

	switch (p_owner->__state) {
	case BIS_LINE:
		if (p_owner->__p_current) {
			lnum++;								/* PARSE AND COMPILE */
			_readLine(p_owner);
			ungot = 0;
			stmt();
			break;
		}

		if (!CSTK_EMPTY) bad("MISSING END");	/* DONE COMPILING */
		emit(BYE_);
		p_owner->_parsing_time = pif_cumulative_timer1ms - p_owner->__start_time;
		p_owner->__state = BIS_RUN;
		break;

	case BIS_RUN:
		// A bounded number of opcodes, which is where the release ends and every other task gets
		// its turn. What is left of the program is in pc, so the next release carries on from here.
		// The program ends only through BYE_, which the compiler puts at its end.
		for (cnt = p_owner->__opcode; cnt > 0; cnt--) (*pc++)();
		break;

	default:
		break;
	}
	return 1;
}

BOOL pifBasic_Init(PifBasicProcess* p_process, PifEvtBasicResult evt_result)
{
	memset(&s_basic, 0, sizeof(PifBasic));
	if (p_process) {
		s_basic.__p_process = p_process;
		s_basic.__process_size = 0;
		while (s_basic.__p_process[s_basic.__process_size]) s_basic.__process_size++;
	}

	s_basic._p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, &s_basic, FALSE);
	if (!s_basic._p_task) return FALSE;
	s_basic._p_task->name = "Basic";
	s_basic.__evt_result = evt_result;
	return TRUE;
}

BOOL pifBasic_Execute(char* p_program, int opcode)
{
	if (!s_basic._p_task || !p_program) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	s_basic.__p_program = p_program;
	if (opcode > 0) s_basic.__opcode = opcode;
	else s_basic.__opcode = PIF_BASIC_OPCODE;
	s_basic.__state = BIS_START;
	pifTask_SetTrigger(s_basic._p_task, 0);
	return TRUE;
}
