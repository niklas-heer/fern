/** Decision105A managed actor ABI; compiler-owned descriptors are never source values. */
#ifndef FERN_MANAGED_H
#define FERN_MANAGED_H
#include <stddef.h>
#include <stdint.h>
typedef struct FernManagedExec FernManagedExec;
typedef struct FernManagedType FernManagedType;
/** Step identity selects one compiler continuation; no native stack survives a suspension. */
typedef struct FernManagedFunction {
    const void* identity;
    int64_t (*step)(FernManagedExec*,void*);
    void* (*select)(FernManagedExec*,void*,int64_t);
    int64_t capture_count;
    const FernManagedType* const* captures;
    const FernManagedType* mailbox;
} FernManagedFunction;
/** Immutable semantic layout, including full-width scalar payloads and tagged products. */
struct FernManagedType {
    int64_t kind;
    int64_t count;
    const FernManagedType* const* children;
    const int64_t* arities;
};
enum { FERN_MANAGED_SCALAR=0, FERN_MANAGED_STRING=1, FERN_MANAGED_LIST=2,
       FERN_MANAGED_PRODUCT=3, FERN_MANAGED_SUM=4, FERN_MANAGED_UNBOXED=5,
       FERN_MANAGED_PID=6, FERN_MANAGED_FUNCTION=7, FERN_MANAGED_JSON=8,
       FERN_MANAGED_MAP=9, FERN_MANAGED_RANGE=10, FERN_MANAGED_UNACCOUNTED=11 };
enum { FERN_MANAGED_RUNNABLE=0, FERN_MANAGED_SUSPENDED=1, FERN_MANAGED_COMPLETE=2, FERN_MANAGED_FAILED=3 };
/** Create one root context borrowing exactly one eight-byte fault slot. */
FernManagedExec* fern_managed_new(int64_t*,const FernManagedFunction* const*,int64_t);
/** Return only the current invocation's eight-byte slot, without a global actor lookup. */
int64_t* fern_managed_fault(FernManagedExec*);
/** Enqueue a validated zero-argument entry without running it inline. */
void* fern_managed_spawn(FernManagedExec*,void*,const FernManagedType*);
/** Borrow the sender's value; heap Result success means enqueue only. */
int64_t fern_managed_send(FernManagedExec*,void*,int64_t,const FernManagedType*);
/** Register one receive, evaluate existing messages first, then yield to the scheduler. */
int64_t fern_managed_receive(FernManagedExec*,void*,void*,int64_t);
/** Replace the current continuation and yield without unwinding an actor stack. */
int64_t fern_managed_continue(FernManagedExec*,void*);
/** Drain actors after successful main, or retire all roots after a prior main failure. */
void fern_managed_run(FernManagedExec*);
/** Stop and clear every invocation-owned frame, timer and mailbox without running source cleanup. */
void fern_managed_stop(FernManagedExec*);
#endif
