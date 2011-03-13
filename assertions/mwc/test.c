/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <stdio.h>

#include "mwc_defs.h"

/*
 * Test program for the 'mwc' assertion.  Run a number of times with various
 * event sequences and see how it works out.
 */

int
main(int argc, char *argv[])
{

	mwc_init();
	mwc_setaction_debug();	/* Use printf(), not assert(). */

	printf("Simulating syscall without check or use; should pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall with successful check, no use; should "
	    "pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 0);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, unsuccessful check, no use; should "
	    "pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 1);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, unsuccessful check, use; should fail\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 1);
	mwc_event_assertion(1, 1);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, use without check; should fail\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_assertion(1, 1);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, check on wrong vnode, use; should "
	    "fail\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(2, 2, 0);
	mwc_event_assertion(1, 1);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, use with check from last run; should "
	    "fail\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_assertion(2, 2);
	mwc_event_tesla_syscall_return();

	printf("Simulating syscall, checking two vnodes and then using "
	    "them; should pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(3, 3, 0);
	mwc_event_mac_vnode_check_write(4, 4, 0);
	mwc_event_assertion(3, 3);
	mwc_event_assertion(4, 4);
	mwc_event_tesla_syscall_return();

	printf("Erroneously enter syscall twice; should pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_tesla_syscall_enter();
	mwc_event_tesla_syscall_return();

	printf("Generating false negative due to out of memory: check(1), "
	    "check(2), use(3); should pass\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 0);
	mwc_event_mac_vnode_check_write(2, 2, 0);
	mwc_event_assertion(3, 3);
	mwc_event_tesla_syscall_return();

	printf("Using same cred twice to make sure both keys used; "
	    "should fail.\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 0);
	mwc_event_assertion(1, 2);
	mwc_event_tesla_syscall_return();

	printf("Using same vnode twice to make sure both keys used; "
	    "should fail.\n");
	mwc_event_tesla_syscall_enter();
	mwc_event_mac_vnode_check_write(1, 1, 0);
	mwc_event_assertion(2, 1);
	mwc_event_tesla_syscall_return();

	mwc_destroy();

	return (0);
}