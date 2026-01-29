/*
 * QBE Compiler Backend - Modified for Fern embedding
 *
 * Original: https://c9x.me/compile/
 * Modified to expose library interface for Fern compiler.
 */

#include "all.h"
#include "config.h"
#include <ctype.h>

Target T;

char debug['Z'+1] = {
	['P'] = 0, /* parsing */
	['M'] = 0, /* memory optimization */
	['N'] = 0, /* ssa construction */
	['C'] = 0, /* copy elimination */
	['F'] = 0, /* constant folding */
	['A'] = 0, /* abi lowering */
	['I'] = 0, /* instruction selection */
	['L'] = 0, /* liveness */
	['S'] = 0, /* spilling */
	['R'] = 0, /* reg. allocation */
};

extern Target T_amd64_sysv;
extern Target T_amd64_apple;
extern Target T_arm64;
extern Target T_arm64_apple;
extern Target T_rv64;

static FILE *outf;
static int dbg;

static void
data(Dat *d)
{
	if (dbg)
		return;
	emitdat(d, outf);
	if (d->type == DEnd) {
		fputs("/* end data */\n\n", outf);
		freeall();
	}
}

static void
func(Fn *fn)
{
	uint n;

	if (dbg)
		fprintf(stderr, "**** Function %s ****", fn->name);
	if (debug['P']) {
		fprintf(stderr, "\n> After parsing:\n");
		printfn(fn, stderr);
	}
	T.abi0(fn);
	fillrpo(fn);
	fillpreds(fn);
	filluse(fn);
	promote(fn);
	filluse(fn);
	ssa(fn);
	filluse(fn);
	ssacheck(fn);
	fillalias(fn);
	loadopt(fn);
	filluse(fn);
	fillalias(fn);
	coalesce(fn);
	filluse(fn);
	ssacheck(fn);
	copy(fn);
	filluse(fn);
	fold(fn);
	T.abi1(fn);
	simpl(fn);
	fillpreds(fn);
	filluse(fn);
	T.isel(fn);
	fillrpo(fn);
	filllive(fn);
	fillloop(fn);
	fillcost(fn);
	spill(fn);
	rega(fn);
	fillrpo(fn);
	simpljmp(fn);
	fillpreds(fn);
	fillrpo(fn);
	assert(fn->rpo[0] == fn->start);
	for (n=0;; n++)
		if (n == fn->nblk-1) {
			fn->rpo[n]->link = 0;
			break;
		} else
			fn->rpo[n]->link = fn->rpo[n+1];
	if (!dbg) {
		T.emitfn(fn, outf);
		fprintf(outf, "/* end function %s */\n\n", fn->name);
	} else
		fprintf(stderr, "\n");
	freeall();
}

static void
dbgfile(char *fn)
{
	emitdbgfile(fn, outf);
}

/*
 * Library interface for Fern compiler.
 * Compiles QBE IR from input FILE* to assembly output FILE*.
 * Returns 0 on success.
 */
int
qbe_compile(FILE *input, FILE *output, const char *filename)
{
	T = Deftgt;
	outf = output;
	dbg = 0;

	/* Reset debug flags */
	for (int i = 0; i <= 'Z'; i++)
		debug[i] = 0;

	parse(input, (char*)filename, dbgfile, data, func);
	T.emitfin(outf);

	return 0;
}

/*
 * Compile QBE IR from a string buffer.
 * Returns 0 on success.
 */
int
qbe_compile_str(const char *ssa_input, FILE *output, const char *filename)
{
	FILE *input = fmemopen((void*)ssa_input, strlen(ssa_input), "r");
	if (!input)
		return 1;

	int result = qbe_compile(input, output, filename);
	fclose(input);
	return result;
}
