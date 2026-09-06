/** Private-state oracles exercise conservative Darwin membership verification. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "fern_runtime.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <libproc.h>
static int listing_bytes;
static int listing_error;
static pid_t listing_member;

/** Supply one bounded synthetic kernel response. @param type/group/buffer/size Query. @return Copied byte count. */
static int test_listpids(uint32_t type, uint32_t group, void* buffer, int size) {
    if (type != PROC_PGRP_ONLY || group != 123 || size != 2 * sizeof(pid_t)) abort();
    ((pid_t*)buffer)[0] = listing_member;
    errno = listing_error;
    return listing_bytes;
}
#define proc_listpids test_listpids
#endif
#include "../../runtime/fern_process.c"

/** Validate complete-only membership, lifecycle gating and error preservation. @return Oracle status. */
int fern_main(void) {
    ExecState state = {.child=123,.retained=true,.finished=true};
#ifdef __APPLE__
    listing_bytes=sizeof(pid_t); listing_member=123; listing_error=0;
    if (!exec_zombie_group(&state)) return 1;
    listing_error=EPERM; if (exec_zombie_group(&state)) return 2;
    listing_error=0; listing_bytes=0; if (exec_zombie_group(&state)) return 3;
    listing_bytes=2*sizeof(pid_t); if (exec_zombie_group(&state)) return 4;
    listing_bytes=-1; if (exec_zombie_group(&state)) return 5;
    listing_bytes=sizeof(pid_t)-1; if (exec_zombie_group(&state)) return 6;
    listing_bytes=sizeof(pid_t); listing_member=124; if (exec_zombie_group(&state)) return 7;
    listing_member=123; state.finished=false; if (exec_zombie_group(&state)) return 8;
    state.finished=true; state.retained=false; if (exec_zombie_group(&state)) return 9;
#else
    if (exec_zombie_group(&state)) return 10;
#endif
    exec_error(&state,FERN_EXEC_TIMEOUT); exec_error(&state,FERN_EXEC_IO);
    if (state.error != FERN_EXEC_TIMEOUT) return 11;
    puts("ok:state");
    return 0;
}
