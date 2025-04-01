#include "core/pif_log.h"
#include "interpreter/pif_basic.h"

#include <ctype.h>
#include <setjmp.h>
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

const char *pun = "(),+-*/\\=<>", *dub = "<><==>";

const char* kwd[] = {
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
static char name[PIF_BASIC_VARIABLE][PIF_BASIC_SYMBOL];								/* VARIABLE NAMES */
static int	sub[PIF_BASIC_VARIABLE][PIF_BASIC_LOCAL + 2];							/* N,LOCAL VAR INDEXES */
static int	mode[PIF_BASIC_VARIABLE];												/* 0=NONE, 1=DIM, 2=SUB */
static Val	ret;																	/* FUNCTION RETURN VALUE */
static int	cstk[PIF_BASIC_STACK], *csp;											/* COMPILER STACK */
static int	cursub, temp, compile, ipc, (**opc)();									/* COMPILER STATE */
static char stab[PIF_BASIC_STRING], *stabp;											/* STRING TABLE */
static jmp_buf	trap;																/* TRAP ERRORS */

#define A		sp[1]					/* LEFT OPERAND */
#define B		sp[0]					/* RIGHT OPERAND */
#define PCV		((Val)*pc++)			/* GET IMMEDIATE */
#define LOC(N)	value[sub[v][N + 2]]	/* SUBROUTINE LOCAL */

int	(*kwdhook)(char *kwd);				/* KEYWORD HOOK */
int	(*funhook)(char *kwd, int n);		/* FUNCTION CALL HOOK */


static PifBasic s_basic;


static void initbasic(int comp)
{
	pc = prg;
	sp = stk + PIF_BASIC_STACK;
	min_sp = sp;
	csp = cstk + PIF_BASIC_STACK;
	stabp = stab;
	cpc = 0;
	compile = comp;
	s_basic._varable_count = 0;
}

static void err(char* msg)
{
	pifLog_Printf(LT_ERROR, "%d: %s", lmap[pc - prg - 1], msg);
	longjmp(trap, 2);
}

static Val* bound(Val* mem, int n)
{
	if (n < 1 || n > *mem) err("BOUNDS");
	return mem + n;
}

static void bad(char* msg)
{
	pifLog_Printf(LT_ERROR, "%d: %s", lnum, msg);
	longjmp(trap, 1);
}

static void emit(int opcode())
{
	lmap[cpc] = lnum;
	prg[cpc++] = opcode;
}

static void inst(int opcode(), Val x)
{
	emit(opcode);
	emit((Code)x);
}

static int BYE_()
{
	longjmp(trap, 4);
}

static int BREAK_()
{
	longjmp(trap, 3);
}

static int RESUME_()
{
	pc = opc ? opc : pc;
	opc = pc;
	cpc = ipc;
	return 1;
}

static int NUMBER_()
{
	*--sp = PCV;
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int LOAD_()
{
	*--sp = value[PCV];
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int STORE_()
{
	value[PCV] = *sp++;
	return 1;
}

static int ECHO_()
{
	pifLog_Printf(LT_INFO, "%d\n", *sp++);
	return 0;
}

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

static int ADD_()
{
	A += B;
	sp++;
	return 1;
}

static int SUBS_()
{
	A -= B;
	sp++;
	return 1;
}

static int MUL_()
{
	A *= B;
	sp++;
	return 1;
}

static int DIV_()
{
	if (!B) {
		sp += 2;
		err("DIVISION BY ZERO");
	}
	A /= B;
	sp++;
	return 1;
}

static int MOD_()
{
	if (!B) {
		sp += 2;
		err("MODULUS OF ZERO");
	}
	A %= B;
	sp++;
	return 1;
}

static int EQ_()
{
	A = (A == B) ? -1 : 0;
	sp++;
	return 1;
}

static int LT_()
{
	A = (A < B) ? -1 : 0;
	sp++;
	return 1;
}

static int GT_()
{
	A = (A > B) ? -1 : 0;
	sp++;
	return 1;
}

static int NE_()
{
	A = (A != B) ? -1 : 0;
	sp++;
	return 1;
}

static int LE_()
{
	A = (A <= B) ? -1 : 0;
	sp++;
	return 1;
}

static int GE_()
{
	A = (A >= B) ? -1 : 0;
	sp++;
	return 1;
}

static int AND_()
{
	A &= B;
	sp++;
	return 1;
}

static int OR_()
{
	A |= B;
	sp++;
	return 1;
}

static int JMP_()
{
	pc = prg + (int)*pc;
	return 1;
}

static int FALSE_()
{
	if (*sp++) pc++;
	else pc = prg + (int)*pc;
	return 1;
}

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

static int NEXT_()
{
	value[PCV]++;
	return 1;
}

static int CALL_()
{
	Val v = PCV, n = sub[v][1], x, *ap = sp;

	while (n--) {
		x = LOC(n);
		LOC(n) = *ap;
		*ap++ = x;
	}
	for (n = sub[v][1]; n < sub[v][0]; n++) *--sp = LOC(n);
	*--sp = pc - prg;
	if (sp < min_sp) min_sp = sp;
	pc = prg + value[v];
	return 1;
}

static int RETURN_()
{
	int v = PCV, n = sub[v][0];

	pc = prg + *sp++;
	while (n--) LOC(n) = *sp++;
	return 1;
}

static int SETRET_()
{
	ret = *sp++;
	return 1;
}

static int RV_()
{
	*--sp = ret;
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int DROP_()
{
	sp += PCV;
	return 1;
}

static int DIM_()
{
	int v = PCV, n = *sp++;
	Val* mem = calloc(sizeof(Val), n + 1);

	if (!mem) err("MEMORY IS LACK");
	mem[0] = n;
	value[v] = (Val)mem;
	return 1;
}

static int LOADI_()
{
	Val x = *sp++;

	x = *bound((Val*)value[PCV], x);
	*--sp = x;
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int STOREI_()
{
	Val x = *sp++, i = *sp++;

	*bound((Val*)value[PCV], i) = x;
	return 1;
}

static int UBOUND_()
{
	*--sp = *(Val*)value[PCV];
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int EXEC_()
{
	int p, i, param[PIF_BASIC_EXEC_SIZE];
	Val n = PCV, *ap = (sp += n) - 1;

	p = *ap - 1;
	ap--;
	if (p >= s_basic.__process_size) err("UNKNOWN PROCESS");
	for (i = 0; i < n - 1; i++) param[i] = *ap--;
	(*s_basic.__p_process[p])(n - 1, param);
	return 1;
}

static int EXECR_()
{
	int p, i, param[PIF_BASIC_EXEC_SIZE];
	Val n = PCV, *ap = (sp += n) - 1;

	p = *ap - 1;
	ap--;
	if (p >= s_basic.__process_size) {
		*--sp = 0;
		if (sp < min_sp) min_sp = sp;
		err("UNKNOWN PROCESS");
	}
	for (i = 0; i < n - 1; i++) param[i] = *ap--;
	*--sp = (*s_basic.__p_process[p])(n - 1, param);
	if (sp < min_sp) min_sp = sp;
	return 1;
}

static int find(char* var)
{
	int	i;

	for (i = 0; i < s_basic._varable_count && strcmp(var, name[i]); i++);
	if (i == s_basic._varable_count) strcpy(name[s_basic._varable_count++], var);
	return i;
}

static int read()											/* READ TOKEN */
{
	char *p;
	const char **k, *d;

	if (ungot) {											/* UNGOT PREVIOUS */
		ungot = 0;
		return tok;
	}
	while (isspace((int)*lp)) lp++;							/* SKIP SPACE */
	if (!*lp || *lp == '#') return tok = 0;					/* END OF LINE */
	if (isdigit((int)*lp)) {								/* NUMBER */
		tokv = strtol(lp, &lp, 0);
		return tok = NUMBER;
	}
	if ((p = strchr(pun, *lp)) && lp++) { 					/* PUNCTUATION */
		for (d = dub; *d && strncmp(d, lp - 1, 2); d += 2);
		if (!*d) return tok = (p - pun) + LP;
		lp++;
		return tok = (d - dub) / 2 + NE;
	}
	else if (isalpha((int)*lp)) {							/* IDENTIFIER */
		for (p = tokn; isalnum((int)*lp); ) *p++ = toupper(*lp++);
		for (*p = 0, k = kwd; *k && strcmp(tokn, *k); k++);
		if (*k) return tok = (k - kwd) + AND;
		tokv = find(tokn);
		return tok = NAME;
	}
	else if (*lp=='"' && lp++) {							/* STRING */
		for (p = stabp; *lp && *lp != '"'; ) *stabp++ = *lp++;
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

static int want(int type)
{
	return !(ungot = read() != type);
}

static void need(int type)
{
	if (!want(type)) bad("SYNTAX ERROR");
}

static int expr();

#define LIST(BODY) if (!want(0)) do { BODY; } while (want(COMMA))

static void base()					/* BASIC EXPRESSION */
{
	int neg = want(SUBS) ? (inst(NUMBER_, 0), 1) : 0;

	if (want(NUMBER)) inst(NUMBER_, tokv);
	else if (want(STRING)) inst(NUMBER_, (Val)(stab + tokv));
	else if (want(NAME)) {
		int var = tokv;
		if (want(LP))
			if (mode[var] == 1) {	/* DIM */
				expr();
				need(RP);
				inst(LOADI_, var);
			}
			else {
				int n = 0;
				LIST(if (tok == RP) break; expr(); n++);
				need(RP);
				if (!funhook || !funhook(name[var], n)) {
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
		need(RP);
		inst(UBOUND_, tokv);
	}
	else if (want(EXEC)) {
		int n = 0;
		LIST(expr(); n++);
		inst(EXECR_, n);
	}
	else bad("BAD EXPRESSION");
	if (neg) emit(SUBS_);			/* NEGATE */
}

static int (*bin[])() = { ADD_, SUBS_, MUL_, DIV_, MOD_, EQ_, LT_, GT_, NE_, LE_, GE_, AND_, OR_};

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

static void stmt()	/* STATEMENT */
{
	int	n, var;

	switch (read()) {
	case FORMAT:
		need(STRING);
		inst(NUMBER_, tokv);
		n = 0;
		if (want(COMMA)) LIST(expr(); n++);
		inst(FORMAT_, n);
		break;

	case SUB:												/* CSTK: {SUB,INDEX,JMP} */
		if (!compile) bad("SUB MUST BE COMPILED");
		compile++;											/* MUST BALANCE WITH END */
		need(NAME);											/* SUB NAME */
		mode[cursub = var = tokv] = 2;
		n = 0; 												/* PARAMS */
		LIST(need(NAME); sub[var][n++ + 2] = tokv);
		*--csp = cpc + 1;									/* JUMP OVER CODE */
		inst(JMP_, 0);
		sub[var][0] = sub[var][1] = n;						/* LOCAL=PARAM COUNT */
		value[var] = cpc;									/* ADDRESS */
		*--csp = var;										/* FOR "END" CLAUSE */
		*--csp = SUB;
		break;

	case LOCAL:
		LIST(need(NAME); sub[cursub][sub[cursub][0]++ + 2] = tokv);
		break;

	case RETURN:
		if (temp) inst(DROP_, temp);
		if (!want(0)) {
			expr();
			emit(SETRET_);
		}
		inst(RETURN_, cursub);
		break;

	case WHILE:												/* CSTK: {WHILE,TEST-FALSE,TOP} */
		compile++;											/* BODY IS COMPILED */
		*--csp = cpc;
		expr();
		*--csp = cpc + 1;
		*--csp = WHILE;
		inst(FALSE_, 0);
		break;

	case FOR:												/* CSTK: {FOR,TEST-FALSE,I,TOP}; STK:{HI} */
		compile++;											/* BODY IS COMPILED */
		need(NAME);
		var = tokv;
		temp++;
		need(EQ);
		expr();
		inst(STORE_, var);
		need(TO);
		expr();
		*--csp = cpc;
		inst(FOR_, var);
		emit(0);
		*--csp = var;
		*--csp = cpc - 1;
		*--csp = FOR;
		break;

	case IF:												/* CSTK: {IF,N,ENDS...,TEST-FALSE} */
		expr();
		inst(FALSE_, 0);
		*--csp = cpc - 1;
		if (want(THEN)) {
			stmt();
			prg[*csp++] = (Code)cpc;
		}
		else {
			compile++;
			*--csp = 0;
			*--csp = IF;
		}
		break;

	case ELSE:
		n = csp[1] + 1;
		inst(JMP_, 0);										/* JUMP OVER "ELSE" */
		*--csp = IF; 										/* ADD A FIXUP */
		csp[1] = n;
		csp[2] = cpc - 1;
		prg[csp[2 + n]] = (Code)cpc;						/* PATCH "ELSE" */
		csp[2 + n] = !want(IF) ? 0 : (expr(), inst(FALSE_, 0), cpc - 1);	/* "ELSE IF" */
		break;

	case END:
		need(*csp++);
		compile--;											/* MATCH BLOCK */
		if (csp[-1] == SUB) {
			inst(RETURN_, *csp++);
			prg[*csp++] = (Code)cpc;						/* PATCH JUMP */
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
			if (!funhook || !funhook(name[var], n)) {
				if (mode[var] != 2 || n != sub[var][1])
					bad("BAD SUB/ARG COUNT");
				inst(CALL_, var);
			}
		}
		break;

	case DIM:
		need(NAME);											/* SET VAR MODE TO DIM */
		mode[var = tokv] = 1;
		need(LP);
		expr();
		need(RP);
		inst(DIM_, var);
		break;

	case RESUME:
		if (!want(0)) expr();
		emit(RESUME_);
		break;

	case BREAK:
		emit(BREAK_);
		break;

	case BYE:
		emit(BYE_);
		break;

	case GT:
		expr();
		emit(ECHO_);
		break;

	case EXEC:
		n = 0;
		LIST(expr(); n++);
		inst(EXEC_, n);
		break;

	default:
		if (tok) bad("BAD STATEMENT");
		break;
	}
	if (!want(0)) bad("TOKENS AFTER STATEMENT");
}

#define USE_KWDHOOK	0

#if USE_KWDHOOK

static int PRINTS_()
{
	pifLog_Print(LT_NONE, (char*)*sp++);
	return 1;
}

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

static uint32_t _doTask(PifTask* p_task)
{
	PifBasic* p_owner = (PifBasic*)p_task->_p_client;
	char *p_current = p_owner->__p_program, *p_nl, *p_cr;
	int n, cnt;

	p_owner->__start_time = pif_cumulative_timer1ms;

	initbasic(1);

#if USE_KWDHOOK
	kwdhook = kwdhook_;
#endif

	int code = setjmp(trap);					/* RETURN ON ERROR */
	if (code == 1) {							/* FILE SYNTAX ERROR */
		p_owner->_result = FALSE;
		goto end;
	}
	if (code == 2) opc = pc;					/* FAULT */
	if (code == 3) {							/* "BREAK" */
		pc = opc ? opc : pc;
		cpc = ipc;
	}
	if (code == 4) {							/* "BYE" */
		p_owner->_result = TRUE;
		goto end;
	}
	while (p_current) {
		p_nl = strchr(p_current, '\n');
		p_cr = strchr(p_current, '\r');
		if (p_nl) {
			if (p_cr) {
				if (p_nl > p_cr) p_nl = p_cr;
				cnt = 2;
			}
			else cnt = 1;
		}
		else {
			if (p_cr) {
				p_nl = p_cr;
				cnt = 1;
			}
			else p_nl = NULL;
		}
		lp = lbuf;
		if (p_nl) {
			n = p_nl - p_current;
			strncpy(lp, p_current, n);
			lp[n] = 0;
			p_current = p_nl + cnt;
		}
		else {
			n = strlen(p_current);
			strncpy(lp, p_current, n);
			lp[n] = 0;
			p_current = NULL;
		}

		lnum++;									/* PARSE AND COMPILE */
		ungot = 0;
		stmt();
		pifTaskManager_Yield();
		if (compile) continue;					/* CONTINUE COMPILING */
		opc = pc;								/* START OF IMMEDIATE */
		pc = prg + ipc;
		emit(BREAK_);
		p_owner->_program_size = cpc;
		p_owner->_string_count = stabp - stab;
		p_owner->_parsing_time = pif_cumulative_timer1ms - p_owner->__start_time;
		cnt = s_basic.__opcode;
		while ((*pc++)()) {						/* RUN STATEMENT */
			if (cnt) cnt--;
			else {
				pifTaskManager_Yield();
				cnt = s_basic.__opcode;
			}
		}
	}
	ipc = cpc + 1;								/* DONE COMPILING */
	compile = 0;
	emit(BYE_);
	p_owner->_program_size = cpc;
	p_owner->_string_count = stabp - stab;
	p_owner->_parsing_time = pif_cumulative_timer1ms - p_owner->__start_time;
	cnt = s_basic.__opcode;
	while ((*pc++)()) {							/* RUN PROGRAM */
		if (cnt) cnt--;
		else {
			pifTaskManager_Yield();
			cnt = s_basic.__opcode;
		}
	}
	p_owner->_result = TRUE;

end:
	p_owner->_stack_count = (int)(stk + PIF_BASIC_STACK - min_sp);
	p_owner->_process_time = pif_cumulative_timer1ms - p_owner->__start_time - p_owner->_parsing_time;

	if (p_owner->__evt_result) (*p_owner->__evt_result)(p_owner);
	return 0;
}

BOOL pifBasic_Init(PifBasicProcess* p_process, PifEvtBasicResult evt_result)
{
	memset(&s_basic, 0, sizeof(PifBasic));
	if (p_process) {
		s_basic.__p_process = p_process;
		s_basic.__process_size = 0;
		while (s_basic.__p_process[s_basic.__process_size]) s_basic.__process_size++;
	}

	s_basic._p_task = pifTaskManager_Add(TM_EXTERNAL_ORDER, 0, _doTask, &s_basic, FALSE);
	if (!s_basic._p_task) return FALSE;
	s_basic._p_task->name = "Basic";
	s_basic.__evt_result = evt_result;
	return TRUE;
}

void pifBasic_Execute(char* p_program, int opcode)
{
	s_basic.__p_program = p_program;
	if (opcode > 0) s_basic.__opcode = opcode;
	else s_basic.__opcode = PIF_BASIC_OPCODE;
	pifTask_SetTrigger(s_basic._p_task, 0);
}
