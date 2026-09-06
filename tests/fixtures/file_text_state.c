/** Inject stream errors while retaining real owned FILE objects and exact close counts. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "fern_runtime.h"
#include "fern_gc.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned mode,opened,closed;

/** Track actual owned stream acquisition. @param path/mode Standard fopen arguments. @return Stream. */
static FILE* test_open(const char* path,const char* mode){FILE* f=fopen(path,mode);if(f)opened++;return f;}
/** Close for real, then optionally simulate a reported late failure. @param file Owned stream. @return Close status. */
static int test_close(FILE* file){closed++;int result=fclose(file);return mode==1||mode==6 ? EOF : result;}
/** Report an injected stream error even when the byte count appears complete. @param file Stream. @return Indicator. */
static int test_error(FILE* file){return mode==2?1:ferror(file);}
/** Inject failed seek or preserve the actual one. @param file Stream. @param offset Offset. @param origin Direction. @return Status. */
static int test_seek(FILE* file,long offset,int origin){return mode==3?-1:fseek(file,offset,origin);}
/** Simulate growth after the original seekable size snapshot. @param file Stream. @return Recorded length. */
static long test_tell(FILE* file){long n=ftell(file);return mode==4?n-1:n;}
/** Fail only the text buffer allocation, preserving heap Result allocation in the linked runtime. @param bytes Size. @return Storage. */
static void* test_allocate(size_t bytes){return mode==6?NULL:GC_MALLOC(bytes);}
#define fopen test_open
#define fclose test_close
#define ferror test_error
#define fseek test_seek
#define ftell test_tell
#undef FERN_ALLOC
#define FERN_ALLOC(bytes) test_allocate(bytes)
#define fern_read_file tested_read_file
#define fern_write_file tested_write_file
#define fern_append_file tested_append_file
#ifndef FILE_TEXT_TEST_MODULE
#define FILE_TEXT_TEST_MODULE "../../runtime/fern_file_text.c"
#endif
#include FILE_TEXT_TEST_MODULE

/** Require one error, one acquisition and one close. @param raw Result. @param code Expected error. */
static void failure(int64_t raw,int code){
    if(fern_result_is_ok(raw)||fern_result_unwrap(raw)!=code||opened!=1||closed!=1){printf("state failure mode%u opens%u closes%u\n",mode,opened,closed);exit(90);}
}
/** Reset counters before each independent owned operation. @param next Injection. */
static void reset(unsigned next){mode=next;opened=0;closed=0;}
/** Check write/read stream completion and all early ownership exits. @return Oracle status. */
int fern_main(void){
    const char* path=fern_arg(1);int fd=open(path,O_CREAT|O_TRUNC|O_WRONLY,0600);if(fd<0)return 91;
    if(write(fd,"okay",4)!=4||close(fd)!=0)return 92;
    reset(1);failure(tested_read_file(path),3);
    reset(1);failure(tested_write_file(path,"okay"),3);
    reset(1);failure(tested_append_file(path,"okay"),3);
    reset(2);failure(tested_read_file(path),3);
    reset(2);failure(tested_write_file(path,"okay"),3);
    reset(2);failure(tested_append_file(path,"okay"),3);
    reset(3);failure(tested_read_file(path),3);
    reset(4);failure(tested_read_file(path),3);
    reset(6);failure(tested_read_file(path),4);
    puts("ok:state");return 0;
}
