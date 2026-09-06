/** File text publication and buffered-completion oracles. */
#define _POSIX_C_SOURCE 200809L
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "fern_runtime.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

/** Fail visibly with a stable source line. @param ok Expected condition. @param line Location. */
static void require_at(bool ok,int line){if(!ok){printf("failure:%d\n",line);fflush(stdout);_exit(90);}}
#define REQUIRE(x) require_at((x),__LINE__)

/** Require an exact heap success/error scalar. @param value Result. @param ok Branch. @param payload Value. */
static void scalar(int64_t value,bool ok,int64_t payload){REQUIRE(fern_result_is_ok(value)==ok);REQUIRE(fern_result_unwrap(value)==payload);}

/** Require the entire original String without representation loss. @param value Result. @param expected Text. */
static void text_result(int64_t value,const char* expected){REQUIRE(fern_result_is_ok(value));REQUIRE(strcmp((char*)(intptr_t)fern_result_unwrap(value),expected)==0);}

/** Write fixture bytes outside the text API. @param path File. @param bytes Data. @param length Byte length. */
static void raw(const char* path,const char* bytes,size_t length){FILE* f=fopen(path,"wb");REQUIRE(f!=NULL);REQUIRE(fwrite(bytes,1,length,f)==length);REQUIRE(fclose(f)==0);}

/** Preserve exact Unicode, empty text and append byte counts. @param path Private output. */
static void valid(const char* path){
    scalar(fern_write_file(path,"🌿\né"),true,7);text_result(fern_read_file(path),"🌿\né");
    scalar(fern_append_file(path,"!"),true,1);text_result(fern_read_file(path),"🌿\né!");
    scalar(fern_write_file(path,""),true,0);text_result(fern_read_file(path),"");
    scalar(fern_read_file("/missing-fern-file-text"),false,1);
    scalar(fern_write_file("/missing-fern-file-text/child","x"),false,2);
}

/** Reject malformed UTF8 and NUL as ordinary read errors, never successful truncated Strings. @param path Fixture. */
static void invalid(const char* path){
    const char* bytes[]={"a\0b","\300\257","\355\240\200","\364\220\200\200","\360\237","\200"};
    const size_t lengths[]={3,2,3,4,2,1};
    for(unsigned i=0;i<6;i++){raw(path,bytes[i],lengths[i]);scalar(fern_read_file(path),false,3);}
    raw(path,"original",8);scalar(fern_write_file(path,"\300\257"),false,3);text_result(fern_read_file(path),"original");
    scalar(fern_append_file(path,"\355\240\200"),false,3);text_result(fern_read_file(path),"original");
}

/** Check exact16MiB boundaries before allocation/open side effects. @param path Private file. */
static void limits(const char* path){
    char* bytes=calloc(16777218,1);REQUIRE(bytes!=NULL);memset(bytes,'x',16777217);
    raw(path,"original",8);scalar(fern_write_file(path,bytes),false,3);text_result(fern_read_file(path),"original");
    scalar(fern_append_file(path,bytes),false,3);text_result(fern_read_file(path),"original");
    bytes[16777216]=0;scalar(fern_write_file(path,bytes),true,16777216);
    int64_t read=fern_read_file(path);REQUIRE(fern_result_is_ok(read));
    const char* text=(char*)(intptr_t)fern_result_unwrap(read);REQUIRE(strlen(text)==16777216 && text[0]=='x' && text[16777215]=='x');
    int fd=open(path,O_WRONLY);REQUIRE(fd>=0);REQUIRE(ftruncate(fd,16777217)==0);REQUIRE(close(fd)==0);
    scalar(fern_read_file(path),false,3);free(bytes);
}

/** Buffered fwrite completion is not successful until fclose's flush succeeds. @param path Private target. */
static void buffered_failure(const char* path){
    struct rlimit old,limited;REQUIRE(getrlimit(RLIMIT_FSIZE,&old)==0);limited=old;limited.rlim_cur=0;
    struct sigaction before,ignore;memset(&ignore,0,sizeof(ignore));sigemptyset(&ignore.sa_mask);ignore.sa_handler=SIG_IGN;
    REQUIRE(sigaction(SIGXFSZ,&ignore,&before)==0);REQUIRE(setrlimit(RLIMIT_FSIZE,&limited)==0);
    scalar(fern_write_file(path,"lost"),false,3);scalar(fern_append_file(path,"lost"),false,3);
    REQUIRE(setrlimit(RLIMIT_FSIZE,&old)==0);REQUIRE(sigaction(SIGXFSZ,&before,NULL)==0);
    struct stat info;REQUIRE(stat(path,&info)==0 && info.st_size==0);
#ifdef __linux__
    scalar(fern_write_file("/dev/full","lost"),false,3);scalar(fern_append_file("/dev/full","lost"),false,3);
#endif
}

/** Select a bounded fixture group. @return Process status. */
int fern_main(void){
    const char* mode=fern_arg(1);const char* path=fern_arg(2);
    if(strcmp(mode,"valid")==0)valid(path);else if(strcmp(mode,"invalid")==0)invalid(path);
    else if(strcmp(mode,"limits")==0)limits(path);else if(strcmp(mode,"buffered")==0)buffered_failure(path);else return 99;
    printf("ok:%s\n",mode);return 0;
}
