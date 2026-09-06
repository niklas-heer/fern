/** Native stderr oracles preserve descriptor, pending-signal and primary-exit state. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "fern_runtime.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern int64_t fern_write_stderr(const char*);
static volatile sig_atomic_t delivered;

/** Fail through stdout so intentionally broken stderr cannot hide an oracle. @param ok/line Condition and location. */
static void require_at(bool ok,int line) { if(!ok){printf("failure:%d\n",line); fflush(stdout); _exit(90);} }
#define REQUIRE(x) require_at((x),__LINE__)

/** Require the exact heap Result branch/payload. @param raw Result. @param error Zero for Unit success. */
static void result(int64_t raw,int error) {
    REQUIRE(fern_result_is_ok(raw)==(error==0)); REQUIRE(fern_result_unwrap(raw)==error);
}

/** Point stderr at a private file and rewind for exact reading. @return Open managed-by-test stream. */
static FILE* capture(void) { FILE* file=tmpfile(); REQUIRE(file!=NULL); REQUIRE(dup2(fileno(file),2)==2); return file; }

/** Check exact bytes, no newline and no stdout redirection. */
static void text(void) {
    FILE* file=capture(); result(fern_write_stderr("error🌿"),0); result(fern_write_stderr(""),0);
    REQUIRE(lseek(fileno(file),0,SEEK_SET)==0); char bytes[32]={0};
    REQUIRE(read(fileno(file),bytes,sizeof(bytes))==9); REQUIRE(strcmp(bytes,"error🌿")==0);
    REQUIRE(fclose(file)==0);
}

/** Closed/readonly stderr fails, while empty output performs no descriptor operation. */
static void closed(void) {
    REQUIRE(close(2)==0); result(fern_write_stderr(""),0); result(fern_write_stderr("error"),3);
    int fd=open("/dev/null",O_RDONLY); REQUIRE(fd>=0);
    if(fd!=2){REQUIRE(dup2(fd,2)==2); REQUIRE(close(fd)==0);}
    result(fern_write_stderr("error"),3); REQUIRE((fcntl(2,F_GETFL)&O_ACCMODE)==O_RDONLY);
}

/** Install a pipe with no readers, retaining only stderr's writer. */
static void broken_pipe(void) {
    int pipefd[2]; REQUIRE(pipe(pipefd)==0); REQUIRE(close(pipefd[0])==0);
    REQUIRE(dup2(pipefd[1],2)==2); if(pipefd[1]!=2) REQUIRE(close(pipefd[1])==0);
}

/** Count externally delivered signals; runtime must not run this handler for its write. @param signal Signal. */
static void handler(int signal) { (void)signal; delivered++; }

/** Compare every supported mask member independently of padding. @param first/second Masks. */
static void same_mask(const sigset_t* first,const sigset_t* second) {
    for(int signal=1;signal<NSIG;signal++) REQUIRE(sigismember(first,signal)==sigismember(second,signal));
}

/** Broken-pipe writes must preserve default/custom/ignored dispositions and exact thread mask. */
static void signals(void) {
    broken_pipe(); struct sigaction old,action,after; memset(&action,0,sizeof(action)); sigemptyset(&action.sa_mask);
    REQUIRE(sigaction(SIGPIPE,NULL,&old)==0);
    void (*handlers[])(int)={SIG_DFL,handler,SIG_IGN};
    for(unsigned i=0;i<3;i++) {
        action.sa_handler=handlers[i]; REQUIRE(sigaction(SIGPIPE,&action,NULL)==0);
        sigset_t before,now; REQUIRE(pthread_sigmask(SIG_SETMASK,NULL,&before)==0);
        result(fern_write_stderr("error"),3);
        REQUIRE(pthread_sigmask(SIG_SETMASK,NULL,&now)==0); same_mask(&before,&now);
        REQUIRE(sigaction(SIGPIPE,NULL,&after)==0); REQUIRE(after.sa_handler==handlers[i]); REQUIRE(delivered==0);
    }
    REQUIRE(sigaction(SIGPIPE,&old,NULL)==0);
}

/** Preserve an already pending signal and a preexisting blocked mask. */
static void pending(void) {
    broken_pipe(); sigset_t pipe,old,pending,now; sigemptyset(&pipe); sigaddset(&pipe,SIGPIPE);
    REQUIRE(pthread_sigmask(SIG_BLOCK,&pipe,&old)==0); REQUIRE(raise(SIGPIPE)==0);
    REQUIRE(sigpending(&pending)==0 && sigismember(&pending,SIGPIPE)==1);
    result(fern_write_stderr("error"),3);
    REQUIRE(sigpending(&pending)==0 && sigismember(&pending,SIGPIPE)==1);
    REQUIRE(pthread_sigmask(SIG_SETMASK,NULL,&now)==0 && sigismember(&now,SIGPIPE)==1);
    int signal; REQUIRE(sigwait(&pipe,&signal)==0 && signal==SIGPIPE);
    REQUIRE(pthread_sigmask(SIG_SETMASK,&old,NULL)==0);
}

/** Nonblocking backpressure is an error and never changes caller flags or reader ownership. */
static void nonblocking(void) {
    int pipefd[2]; REQUIRE(pipe(pipefd)==0); int flags=fcntl(pipefd[1],F_GETFL); REQUIRE(flags>=0);
    REQUIRE(fcntl(pipefd[1],F_SETFL,flags|O_NONBLOCK)==0); REQUIRE(dup2(pipefd[1],2)==2);
    char bytes[4096]; memset(bytes,'x',sizeof(bytes));
    while(write(2,bytes,sizeof(bytes))>0) {} REQUIRE(errno==EAGAIN);
    flags=fcntl(2,F_GETFL); REQUIRE(flags>=0 && (flags&O_NONBLOCK));
    result(fern_write_stderr("error"),3); REQUIRE(fcntl(2,F_GETFL)==flags);
    REQUIRE(read(pipefd[0],bytes,sizeof(bytes))>0); REQUIRE(close(pipefd[0])==0); REQUIRE(close(pipefd[1])==0);
}

/** Validate text and byte limits before output, and accept the exact16MiB bound. */
static void limits(void) {
    FILE* file=capture(); result(fern_write_stderr(NULL),1); result(fern_write_stderr("\300\257"),1);
    result(fern_write_stderr("\355\240\200"),1); result(fern_write_stderr("\364\220\200\200"),1);
    result(fern_write_stderr("\360\237"),1); REQUIRE(lseek(fileno(file),0,SEEK_CUR)==0);
    char* bytes=calloc(16777218,1); REQUIRE(bytes!=NULL); memset(bytes,'x',16777217);
    result(fern_write_stderr(bytes),2); REQUIRE(lseek(fileno(file),0,SEEK_CUR)==0);
    bytes[16777216]=0; result(fern_write_stderr(bytes),0); REQUIRE(lseek(fileno(file),0,SEEK_CUR)==16777216);
    free(bytes); REQUIRE(fclose(file)==0);
}

typedef struct { int fd; size_t bytes; int error; } Drain;

/** Drain one pipe without changing this separate thread's inherited signal mask. @param data Drain state. @return NULL. */
static void* drain(void* data) {
    Drain* state=data; char bytes[8192]; sigset_t original,now;
    if(pthread_sigmask(SIG_SETMASK,NULL,&original)!=0){state->error=1; return NULL;}
    for(;;) {
        ssize_t count=read(state->fd,bytes,sizeof(bytes));
        if(count<0 && errno==EINTR) continue;
        if(count<=0){if(count<0)state->error=2; break;}
        state->bytes+=(size_t)count;
        if(pthread_sigmask(SIG_SETMASK,NULL,&now)!=0 || sigismember(&now,SIGPIPE)!=sigismember(&original,SIGPIPE)) state->error=3;
    }
    if(close(state->fd)!=0) state->error=4;
    return NULL;
}

/** Complete large pipe writes with a real reader; other descriptors and thread masks remain independent. */
static void concurrent(void) {
    struct stat before[2],after;
    for(int fd=0;fd<2;fd++) REQUIRE(fstat(fd,&before[fd])==0);
    int saved=dup(2),pipefd[2]; REQUIRE(saved>=0); REQUIRE(pipe(pipefd)==0);
    REQUIRE(dup2(pipefd[1],2)==2); REQUIRE(close(pipefd[1])==0);
    Drain state={pipefd[0],0,0}; pthread_t reader; REQUIRE(pthread_create(&reader,NULL,drain,&state)==0);
    char* text=calloc(1024*1024+1,1); REQUIRE(text!=NULL); memset(text,'x',1024*1024);
    result(fern_write_stderr(text),0); free(text);
    REQUIRE(dup2(saved,2)==2); REQUIRE(close(saved)==0); REQUIRE(pthread_join(reader,NULL)==0);
    REQUIRE(state.error==0 && state.bytes==1024*1024);
    for(int fd=0;fd<2;fd++){REQUIRE(fstat(fd,&after)==0); REQUIRE(before[fd].st_dev==after.st_dev && before[fd].st_ino==after.st_ino);}
}

/** Preserve the CLI's primary exit status after reporting fails. @return Original failure7. */
static int primary(void) { broken_pipe(); result(fern_write_stderr("primary error"),3); return 7; }

/** Select one independently bounded native test. @return Status. */
int fern_main(void) {
    const char* mode=fern_arg(1);
    if(strcmp(mode,"text")==0) text(); else if(strcmp(mode,"closed")==0) closed();
    else if(strcmp(mode,"signals")==0) signals(); else if(strcmp(mode,"pending")==0) pending();
    else if(strcmp(mode,"nonblocking")==0) nonblocking(); else if(strcmp(mode,"limits")==0) limits();
    else if(strcmp(mode,"concurrent")==0) concurrent();
    else if(strcmp(mode,"primary")==0) return primary(); else return 99;
    printf("ok:%s\n",mode); return 0;
}
