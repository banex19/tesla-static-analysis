//! @file static.c  Tests instrumentation of static functions.
/*
 * Commands for llvm-lit:
 * RUN: tesla analyse %s -o %t.tesla -- %cflags
 * RUN: clang -S -emit-llvm %cflags %s -o %t.ll
 * RUN: tesla instrument -tesla-manifest %t.tesla %t.ll -o %t.instr.ll
 * RUN: clang %ldflags %t.instr.ll -o %t
 * RUN: %t | tee %t.out
 * RUN: FileCheck -input-file %t.out %s
 */

#include "tesla-macros.h"

static void foo(void) {}
static void bar(void) {}

int
main(int argc, char *argv[])
{
	/*
	 * We should update state on entering main():
	 *
	 * CHECK: ====
	 * CHECK: tesla_update_state
	 * CHECK: per-thread
	 * CHECK: transitions:  [ (0:0x0 -> 1 <init>) ]
	 * CHECK: key:          0x0 [ X X X X ]
	 * CHECK: ====
	 */


	// Update state again on calling foo():
	foo();
	/*
	 * CHECK: ====
	 * CHECK: tesla_update_state
	 * CHECK: per-thread
	 * CHECK: transitions:  [ (1:0x0 -> 2) ]
	 * CHECK: key:          0x0 [ X X X X ]
	 * CHECK: ====
	 */


	// Again on calling bar():
	bar();
	/*
	 * CHECK: ====
	 * CHECK: tesla_update_state
	 * CHECK: per-thread
	 * CHECK: transitions:  [ (2:0x0 -> 3) ]
	 * CHECK: key:          0x0 [ X X X X ]
	 * CHECK: ====
	 */


	// And finally, on the NOW event:
	TESLA_WITHIN(main,
		TSEQUENCE(
			caller(called(foo)),
			callee(called(bar)),
			TESLA_NOW
		)
	);
	/*
	 * CHECK: ====
	 * CHECK: tesla_update_state
	 * CHECK: per-thread
	 * CHECK: transitions:  [ (3:0x0 -> 4) ]
	 * CHECK: key:          0x0 [ X X X X ]
	 * CHECK: pass '{{.*}}': {{[0-9]+}}
	 * CHECK: ====
	 */

	return 0;
}

