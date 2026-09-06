/** Explicit fallible stderr writes; Decision87 preserves descriptors and thread-local signals. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "fern_runtime.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define STDERR_BYTES (16u * 1024u * 1024u)
#define STDERR_CHUNK 16384u
#define STDERR_ATTEMPTS 65536u

/** Validate full scalar text before output. @param text Bytes. @param length Bounded byte count. @return Valid UTF8. */
static bool stderr_utf8(const char* text,size_t length) {
    assert(text != NULL);
    assert(length <= STDERR_BYTES);
    for(size_t i=0;i<length;) {
        unsigned char a=(unsigned char)text[i++];
        if(a<0x80) continue;
        unsigned extra=a>=0xc2 && a<=0xdf ? 1 : a>=0xe0 && a<=0xef ? 2 : a>=0xf0 && a<=0xf4 ? 3 : 0;
        if(extra==0 || extra>length-i) return false;
        unsigned char b=(unsigned char)text[i];
        if((a==0xe0 && b<0xa0) || (a==0xed && b>=0xa0) || (a==0xf0 && b<0x90) || (a==0xf4 && b>=0x90)) return false;
        for(unsigned j=0;j<extra;j++) { unsigned char byte=(unsigned char)text[i++]; if(byte<0x80 || byte>0xbf) return false; }
    }
    return true;
}

/** Inspect pending SIGPIPE without changing it. @param pending Output membership. @return Zero or IO error. */
static int stderr_pending(bool* pending) {
    assert(pending != NULL);
    sigset_t signals;
    if(sigpending(&signals)!=0) return FERN_STDERR_IO;
    int member=sigismember(&signals,SIGPIPE);
    if(member<0) return FERN_STDERR_IO;
    assert(member==0 || member==1);
    *pending=member!=0;
    return 0;
}

/** Consume only a confirmed newly generated SIGPIPE; competing signal consumers are excluded.
 * @param pipe Set containing only SIGPIPE. @return Zero or IO error.
 */
static int stderr_consume(const sigset_t* pipe) {
    assert(pipe != NULL);
    assert(sigismember(pipe,SIGPIPE)==1);
    bool pending=false;
    if(stderr_pending(&pending)!=0) return FERN_STDERR_IO;
    if(!pending) return 0;
#ifdef __APPLE__
    int signal=0;
    return sigwait(pipe,&signal)==0 && signal==SIGPIPE ? 0 : FERN_STDERR_IO;
#else
    struct timespec zero={0,0};
    int signal=sigtimedwait(pipe,NULL,&zero);
    return signal==SIGPIPE || (signal<0 && errno==EAGAIN) ? 0 : FERN_STDERR_IO;
#endif
}

/** Write bounded chunks and charge all attempts, including EINTR and partial progress.
 * @param text Validated bytes. @param length Byte count. @param broken Whether EPIPE occurred.
 * @return Zero on complete output or IO error; partial output cannot be rolled back.
 */
static int stderr_write(const char* text,size_t length,bool* broken) {
    assert(text != NULL && broken != NULL);
    assert(length > 0 && length <= STDERR_BYTES);
    size_t offset=0;
    for(unsigned attempt=0;attempt<STDERR_ATTEMPTS && offset<length;attempt++) {
        size_t count=length-offset; if(count>STDERR_CHUNK) count=STDERR_CHUNK;
        ssize_t written=write(STDERR_FILENO,text+offset,count);
        if(written>0) offset+=(size_t)written;
        else if(written==0) return FERN_STDERR_IO;
        else if(errno!=EINTR) { *broken=errno==EPIPE; return FERN_STDERR_IO; }
    }
    return offset==length ? 0 : FERN_STDERR_IO;
}

/** Mask only this thread's SIGPIPE and restore its original pending/mask state on every path.
 * @param text Validated nonempty input. @param length Bounded byte count. @return Zero or IO error.
 */
static int stderr_output(const char* text,size_t length) {
    assert(text != NULL);
    assert(length > 0 && length <= STDERR_BYTES);
    sigset_t pipe,old;
    if(sigemptyset(&pipe)!=0 || sigaddset(&pipe,SIGPIPE)!=0) return FERN_STDERR_IO;
    if(pthread_sigmask(SIG_BLOCK,&pipe,&old)!=0) return FERN_STDERR_IO;
    bool pending=false,broken=false;
    int error=stderr_pending(&pending);
    if(error==0) error=stderr_write(text,length,&broken);
    if(broken && !pending && stderr_consume(&pipe)!=0) error=FERN_STDERR_IO;
    if(pthread_sigmask(SIG_SETMASK,&old,NULL)!=0) error=FERN_STDERR_IO;
    return error;
}

/** Write exact UTF8 stderr text without inserting a newline or changing caller descriptors.
 * @param text Native CString, at most16MiB. @return Heap Result: Ok(Unit0), Err(stable code).
 */
int64_t fern_write_stderr(const char* text) {
    if(text==NULL) return fern_result_err(FERN_STDERR_INVALID);
    size_t length=strnlen(text,STDERR_BYTES+1);
    if(length>STDERR_BYTES) return fern_result_err(FERN_STDERR_LIMIT);
    if(!stderr_utf8(text,length)) return fern_result_err(FERN_STDERR_INVALID);
    assert(length<=STDERR_BYTES);
    assert(text[length]==0);
    int error=length==0 ? 0 : stderr_output(text,length);
    return error==0 ? fern_result_ok(0) : fern_result_err(error);
}
