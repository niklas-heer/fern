/** Independent native continuation oracles; no source compiler can fake scheduler success. */
#include "fern_managed.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(condition) do { if(!(condition)) { fprintf(stderr,"check failed at line %d: %s\n",__LINE__,#condition); exit(1); } } while(0)
static int events[32]; static size_t count;
static FernManagedType scalar={FERN_MANAGED_SCALAR,0,NULL,NULL};
static void* blocks[100000]; static size_t block_count;
/** Test allocator mirrors shared ownership and retains allocations until the test ends. */
void* fern_alloc(size_t size) { assert(block_count<100000); void* p=calloc(1,size); assert(p); blocks[block_count++]=p; return p; }
/** Test Result ABI matches the production heap tag/payload representation. */
int64_t fern_result_ok(int64_t value) { int64_t* p=fern_alloc(16); p[0]=0; p[1]=value; return (int64_t)(intptr_t)p; }
int64_t fern_result_err(int64_t value) { int64_t* p=fern_alloc(16); p[0]=1; p[1]=value; return (int64_t)(intptr_t)p; }
static int64_t worker(FernManagedExec* exec,void* env) { (void)exec; (void)env; events[count++]=2; return FERN_MANAGED_COMPLETE; }
static void* next_frame;
static int64_t receiver(FernManagedExec* exec,void* env) { (void)env; events[count++]=1; return fern_managed_receive(exec,next_frame,NULL,-1); }
static int64_t selected(FernManagedExec* exec,void* env) { (void)exec; events[count++]=(int)((intptr_t*)env)[1]; return FERN_MANAGED_COMPLETE; }
static void* selector(FernManagedExec* exec,void* env,int64_t value);
static FernManagedFunction f_worker={(void*)worker,worker,NULL,0,NULL,NULL};
static FernManagedFunction f_receiver={(void*)receiver,receiver,NULL,0,NULL,&scalar};
static const FernManagedType* captures[]={&scalar};
static FernManagedFunction f_selected={(void*)selected,selected,NULL,1,captures,&scalar};
static FernManagedFunction f_selector={(void*)selector,NULL,selector,0,NULL,&scalar};
static void* selector(FernManagedExec* exec,void* env,int64_t value) { (void)exec; (void)env; if(value!=42) return NULL; intptr_t* p=fern_alloc(16); p[0]=(intptr_t)selected; p[1]=(intptr_t)value; return p; }
static const FernManagedFunction* functions[]={&f_worker,&f_receiver,&f_selected,&f_selector};
static void* closure(const void* code) { const void** p=fern_alloc(8); *p=code; return p; }
static void success(int64_t result) { assert(((int64_t*)(intptr_t)result)[0]==0); }
int main(void) {
    int64_t fault=0;
    FernManagedExec* root=fern_managed_new(&fault,functions,4); assert(root && fault==0);
    fern_managed_spawn(root,closure((void*)worker),&scalar); assert(count==0);
    fern_managed_run(root); assert(fault==0 && count==1 && events[0]==2);
    fern_managed_stop(root);
    count=0; fault=0; root=fern_managed_new(&fault,functions,4);
    next_frame=closure((void*)selector);
    void* pid=fern_managed_spawn(root,closure((void*)receiver),&scalar);
    success(fern_managed_send(root,pid,9,&scalar)); success(fern_managed_send(root,pid,42,&scalar));
    fern_managed_run(root); assert(fault==0 && count==2 && events[0]==1 && events[1]==42);
    int64_t result=fern_managed_send(root,pid,42,&scalar); assert(((int64_t*)(intptr_t)result)[0]==1 && ((int64_t*)(intptr_t)result)[1]==3);
    fern_managed_stop(root);
    for(size_t i=0;i<block_count;i++) free(blocks[i]);
    puts("managed scheduler FIFO/selection/stale identity: ok"); return 0;
}
