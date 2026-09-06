/** Native Decision85 oracles; child helper modes use literal argv and no shell. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "fern_runtime.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* The missing symbol supplies the initial linker-level red before implementation. */
extern int64_t fern_exec_args_bounded(FernStringList*, int64_t, int64_t);
static int report_fd = STDOUT_FILENO;
static const char* self;

/** Fail visibly even when caller stdio is intentionally closed. @param ok Expected condition. @param line Oracle location. */
static void require_at(bool ok, int line) {
    if (!ok) { dprintf(report_fd, "failure:%d\n", line); _exit(90); }
}
#define REQUIRE(value) require_at((value), __LINE__)

/** Pause a bounded number of milliseconds despite test signals. @param ms Duration. */
static void pause_ms(long ms) {
    struct timespec delay = {ms / 1000, (ms % 1000) * 1000000};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
}

/** Write exact byte chunks through blocking child streams. @param fd Stream. @param data/len Bytes. */
static void put(int fd, const char* data, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, data, len);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) _exit(91);
        data += n; len -= (size_t)n;
    }
}

/** Build one stack-native list. @param argv/n Literal arguments. @param timeout/cap Limits. @return Heap Result. */
static int64_t invoke(char** argv, int64_t n, int64_t timeout, int64_t cap) {
    FernStringList list = {argv, n, n};
    return fern_exec_args_bounded(&list, timeout, cap);
}

/** Execute a helper. @param mode Child behavior. @param extra Optional literal argument. @param timeout/cap Limits. @return Heap Result. */
static int64_t child(const char* mode, const char* extra, int64_t timeout, int64_t cap) {
    char* argv[] = {(char*)self, "child", (char*)mode, (char*)extra};
    return invoke(argv, extra == NULL ? 3 : 4, timeout, cap);
}

/** Require a normal status and independent exact streams. @param raw Result. @param code Status. @param out/err Text. */
static void success(int64_t raw, int code, const char* out, const char* err) {
    if (!fern_result_is_ok(raw)) dprintf(report_fd,"unexpected error:%lld\n",(long long)fern_result_unwrap(raw));
    REQUIRE(fern_result_is_ok(raw));
    FernExecResult* value = (FernExecResult*)(intptr_t)fern_result_unwrap(raw);
    REQUIRE(value != NULL);
    REQUIRE(value->exit_code == code);
    REQUIRE(strcmp(value->stdout_str, out) == 0);
    REQUIRE(strcmp(value->stderr_str, err) == 0);
}

/** Require a specific stable failure. @param raw Result. @param code Error. */
static void failure(int64_t raw, int code) {
    REQUIRE(!fern_result_is_ok(raw));
    if (fern_result_unwrap(raw) != code) dprintf(report_fd,"expected error:%d actual:%lld\n",code,(long long)fern_result_unwrap(raw));
    REQUIRE(fern_result_unwrap(raw) == code);
}

/** Generate split/binary stream payloads. @param mode Named fixture. @param extra Optional amount. @return Exit status. */
static int child_bytes(const char* mode, const char* extra) {
    if (strcmp(mode, "bytes") == 0 || strcmp(mode, "errbytes") == 0) {
        long count = strtol(extra, NULL, 10); char bytes[4096]; memset(bytes, 'x', sizeof(bytes));
        while (count > 0) { size_t n = count > 4096 ? 4096 : (size_t)count; put(strcmp(mode, "errbytes") == 0 ? 2 : 1, bytes, n); count -= (long)n; }
    } else if (strcmp(mode, "stderr") == 0) put(2, extra, strlen(extra));
    else if (strcmp(mode, "split") == 0) { put(1, "\360\237", 2); pause_ms(5); put(1, "\214\277", 2); }
    else if (strcmp(mode, "nul") == 0) put(1, "a\0b", 3);
    else if (strcmp(mode, "invalid") == 0) put(2, "\300\257", 2);
    else if (strcmp(mode, "surrogate") == 0) put(1, "\355\240\200", 3);
    else if (strcmp(mode, "range") == 0) put(2, "\364\220\200\200", 4);
    else if (strcmp(mode, "continuation") == 0) put(1, "\200", 1);
    else if (strcmp(mode, "truncated") == 0) put(1, "\360\237", 2);
    else if (strcmp(mode, "flood") == 0) {
        char bytes[4096]; memset(bytes, 'x', sizeof(bytes));
        for (unsigned i = 0; i < 100000; i++) { put(1, bytes, sizeof(bytes)); put(2, "e", 1); }
    } else return -1;
    return 0;
}

/** Emit heartbeats from a same-group grandchild. @param path Private test marker. @return Parent exit. */
static int descendant(const char* path) {
    int ready[2]; if (pipe(ready) != 0) return 92;
    pid_t pid = fork(); if (pid < 0) return 92;
    if (pid == 0) {
        close(ready[0]);
        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) _exit(93);
        put(fd, "x", 1); put(ready[1], "r", 1); close(ready[1]);
        for (unsigned i = 0; i < 2000; i++) { put(fd, "x", 1); pause_ms(2); }
        _exit(0);
    }
    close(ready[1]); char byte;
    if (read(ready[0], &byte, 1) != 1) return 93;
    close(ready[0]);
    return 0;
}

/** Leave a bounded escaped writer to prove capture does not wait for inherited EOF. @return Parent status. */
static int escaped_writer(void) {
    int ready[2]; if(pipe(ready)!=0) return 92;
    pid_t pid=fork(); if(pid<0) return 92;
    if(pid==0) {
        close(ready[0]); if(setsid()<0) _exit(93);
        put(ready[1],"r",1); close(ready[1]); pause_ms(2000); _exit(0);
    }
    close(ready[1]); char byte;
    if(read(ready[0],&byte,1)!=1) return 93;
    close(ready[0]); dprintf(1,"%d",pid); return 0;
}

/** Implement child fixtures without invoking the runtime process API recursively. @return Child status. */
static int child_main(void) {
    const char* mode = fern_arg(2); const char* extra = fern_arg(3);
    int bytes = child_bytes(mode, extra); if (bytes >= 0) return bytes;
    if (strcmp(mode, "literal") == 0) { put(1, extra, strlen(extra)); return 0; }
    if (strcmp(mode, "streams") == 0) { put(1, "out", 3); put(2, "err", 3); return 7; }
    if (strcmp(mode, "exit127") == 0) return 127;
    if (strcmp(mode, "name") == 0) { put(1,fern_arg(0),strlen(fern_arg(0))); return 0; }
    if (strcmp(mode, "empty") == 0) return 0;
    if (strcmp(mode, "stdin") == 0) { char c; return read(0, &c, 1) == 0 ? 0 : 94; }
    if (strcmp(mode, "signal") == 0) { raise(SIGTERM); return 95; }
    if (strcmp(mode, "sleep") == 0) { pause_ms(5000); return 0; }
    if (strcmp(mode, "closed") == 0) { close(1); close(2); pause_ms(5000); return 0; }
    if (strcmp(mode, "descendant") == 0) return descendant(extra);
    if (strcmp(mode, "escaped") == 0) return escaped_writer();
    if (strcmp(mode, "environment") == 0) { const char* text = getenv("FERN_PROCESS_TEST"); if (text) put(1, text, strlen(text)); return 0; }
    if (strcmp(mode, "fds") == 0) { for (int fd = 3; fd < 256; fd++) if (fcntl(fd, F_GETFD) != -1) return 96; return 0; }
    if (strcmp(mode, "group") == 0) return getpgrp() == getpid() ? 0 : 97;
    return 98;
}

/** Confirm literal argv, independent streams, inherited environment and full-width heap transport. */
static void basic(void) {
    success(child("literal", "literal ; $(no) $HOME `no` ' \" 🌿", 1000, 1024), 0, "literal ; $(no) $HOME `no` ' \" 🌿", "");
    success(child("literal", "", 1000, 0), 0, "", "");
    success(child("streams", NULL, 1000, 3), 7, "out", "err");
    success(child("exit127", NULL, 1000, 0), 127, "", "");
    success(child("stdin", NULL, 1000, 0), 0, "", "");
    success(child("group", NULL, 1000, 0), 0, "", "");
    REQUIRE(setenv("FERN_PROCESS_TEST", "kept🌿", 1) == 0);
    success(child("environment", NULL, 1000, 128), 0, "kept🌿", "");
    int64_t result = child("empty", NULL, 1000, 0);
    int64_t pointer = fern_result_unwrap(result);
    REQUIRE(fern_result_unwrap(fern_result_ok(pointer)) == pointer);
    REQUIRE(fern_result_unwrap(fern_result_ok(INT64_C(4294967297))) == INT64_C(4294967297));
    REQUIRE(sizeof(FernExecResult) == 24);
}

/** Exercise each independent cap, buffered HUP bytes and split UTF8 scalar boundaries. */
static void output(void) {
    success(child("literal", "x", 1000, 1), 0, "x", "");
    failure(child("literal", "x", 1000, 0), 4);
    failure(child("literal", "xx", 1000, 1), 4);
    success(child("stderr", "x", 1000, 1), 0, "", "x");
    failure(child("stderr", "xx", 1000, 1), 4);
    char expected[4097]; memset(expected, 'x', 4096); expected[4096] = 0;
    success(child("bytes", "4096", 1000, 4096), 0, expected, "");
    failure(child("bytes", "4097", 1000, 4096), 4);
    success(child("errbytes", "4096", 1000, 4096), 0, "", expected);
    failure(child("errbytes", "4097", 1000, 4096), 4);
    success(child("split", NULL, 1000, 4), 0, "🌿", "");
    failure(child("split", NULL, 1000, 3), 4);
    failure(child("nul", NULL, 1000, 100), 6);
    failure(child("invalid", NULL, 1000, 100), 6);
    failure(child("truncated", NULL, 1000, 100), 6);
    failure(child("surrogate", NULL, 1000, 100), 6);
    failure(child("range", NULL, 1000, 100), 6);
    failure(child("continuation", NULL, 1000, 100), 6);
    failure(child("flood", NULL, 1000, 8192), 4);
}

/** Reject malformed list headers, argv and all full-width invalid limits before spawning. */
static void invalid(void) {
    char* argv[] = {(char*)self, "child", "empty"};
    const int64_t bad[] = {0, -1, INT64_C(4294967297), -INT64_C(4294967295), INT64_MAX, INT64_MIN};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) failure(invoke(argv, 3, bad[i], 0), 1);
    for (unsigned i = 1; i < sizeof(bad)/sizeof(bad[0]); i++) failure(invoke(argv, 3, 1000, bad[i]), 1);
    failure(invoke(argv, 3, 600001, 0), 1); failure(invoke(argv, 3, 1000, 16777217), 1);
    FernStringList lists[] = {{NULL,1,1},{argv,-1,1},{argv,3,2},{argv,4097,4097},{argv,0,0}};
    failure(fern_exec_args_bounded(NULL,1000,0),1);
    for (unsigned i = 0; i < sizeof(lists)/sizeof(lists[0]); i++) failure(fern_exec_args_bounded(&lists[i],1000,0),1);
    char* empty[] = {""}; failure(invoke(empty,1,1000,0),1);
    char* utf8[] = {(char*)self,"child","literal","\300\257"}; failure(invoke(utf8,4,1000,100),1);
    char* null[] = {(char*)self,NULL}; failure(invoke(null,2,1000,0),1);
}

/** Keep normal127 distinct from synchronous spawn errors and never use an implicit shell. */
static void spawn_errors(void) {
    char* missing[] = {"/definitely-not-a-fern-program"}; failure(invoke(missing,1,1000,0),2);
    char* args[] = {"printf", "%s", "PATH literal"}; success(invoke(args,3,1000,32),0,"PATH literal","");
    const char* path = fern_arg(2);
    int fd = open(path,O_CREAT|O_TRUNC|O_WRONLY,0700); REQUIRE(fd >= 0);
    put(fd,"echo IMPLICIT_SHELL\n",20); REQUIRE(close(fd) == 0);
    char* invalid[] = {(char*)path}; int64_t raw = invoke(invalid,1,1000,128);
    if (fern_result_is_ok(raw)) success(raw,127,"",""); else failure(raw,2);
}

/** Verify timeout/signal precedence and group cleanup without touching an unrelated sibling. */
static void lifecycle(void) {
    failure(child("sleep",NULL,20,0),3);
    failure(child("closed",NULL,20,0),3);
    failure(child("signal",NULL,1000,0),7);
    pid_t sibling = fork(); REQUIRE(sibling >= 0);
    if (sibling == 0) { pause_ms(3000); _exit(0); }
    pid_t group = getpgrp();
    success(child("descendant",fern_arg(2),1000,128),0,"","");
    REQUIRE(getpgrp() == group); REQUIRE(kill(sibling,0) == 0);
    struct stat before, after; REQUIRE(stat(fern_arg(2),&before) == 0);
    pause_ms(40); REQUIRE(stat(fern_arg(2),&after) == 0); REQUIRE(before.st_size == after.st_size);
    REQUIRE(kill(sibling,SIGKILL) == 0); int status; REQUIRE(waitpid(sibling,&status,0) == sibling);
}

/** Require successful finite capture despite a deliberately escaped open writer. */
static void escaped(void) {
    int64_t raw=child("escaped",NULL,1000,128); REQUIRE(fern_result_is_ok(raw));
    FernExecResult* value=(FernExecResult*)(intptr_t)fern_result_unwrap(raw);
    REQUIRE(value->exit_code==0); REQUIRE(value->stderr_str[0]==0);
    char* end; long pid=strtol(value->stdout_str,&end,10); REQUIRE(pid>1 && *end==0);
    REQUIRE(kill((pid_t)pid,0)==0); REQUIRE(kill((pid_t)pid,SIGKILL)==0);
}

/** Exercise the largest exact cap and one-byte overflow without a second large expected string. */
static void maximum_output(void) {
    int64_t raw=child("bytes","16777216",5000,16777216); REQUIRE(fern_result_is_ok(raw));
    FernExecResult* value=(FernExecResult*)(intptr_t)fern_result_unwrap(raw);
    REQUIRE(value->exit_code==0); REQUIRE(value->stderr_str[0]==0);
    REQUIRE(strlen(value->stdout_str)==16777216);
    REQUIRE(value->stdout_str[0]=='x' && value->stdout_str[16777215]=='x');
    failure(child("bytes","16777217",5000,16777216),4);
}

/** Bound PATH metadata and preserve search order, empty components, slash bypass and argv0. */
static void path_search(void) {
    const char* original=getenv("PATH"); char* saved=original==NULL ? NULL : strdup(original);
    const char* directory=fern_arg(2); REQUIRE(mkdir(directory,0700)==0);
    char executable[4096]; REQUIRE(snprintf(executable,sizeof(executable),"%s/tool",directory)>0);
    REQUIRE(symlink(self,executable)==0);
    char search[8192]; REQUIRE(snprintf(search,sizeof(search),"/missing-fern:%s",directory)>0);
    REQUIRE(setenv("PATH",search,1)==0); char* args[]={"tool","child","empty"};
    success(invoke(args,3,2000,0),0,"","");
    args[2]="name"; success(invoke(args,3,2000,32),0,"tool",""); args[2]="empty";
    char cwd[4096]; REQUIRE(getcwd(cwd,sizeof(cwd))!=NULL); REQUIRE(chdir(directory)==0);
    REQUIRE(setenv("PATH",":/missing-fern",1)==0); success(invoke(args,3,2000,0),0,"","");
    REQUIRE(chdir(cwd)==0); REQUIRE(unsetenv("PATH")==0);
    char* printf_args[]={"printf","%s","default"}; success(invoke(printf_args,3,1000,32),0,"default","");
    char* long_path=calloc(1024*1024+2,1); REQUIRE(long_path!=NULL);
    memset(long_path,'x',1024*1024); REQUIRE(setenv("PATH",long_path,1)==0);
    failure(invoke(args,3,2000,0),2);
    long_path[1024*1024]='x'; REQUIRE(setenv("PATH",long_path,1)==0); failure(invoke(args,3,2000,0),1);
    memset(long_path,':',4095); long_path[4095]=0; REQUIRE(setenv("PATH",long_path,1)==0);
    REQUIRE(chdir(directory)==0); success(invoke(args,3,2000,0),0,"",""); REQUIRE(chdir(cwd)==0);
    char* direct[]={(char*)self,"child","empty"}; success(invoke(direct,3,2000,0),0,"","");
    long_path[4095]=':'; long_path[4096]=0; REQUIRE(setenv("PATH",long_path,1)==0);
    failure(invoke(args,3,2000,0),1); success(invoke(direct,3,2000,0),0,"","");
    free(long_path);
    if(saved!=NULL) { REQUIRE(setenv("PATH",saved,1)==0); free(saved); } else REQUIRE(unsetenv("PATH")==0);
}

/** Verify every caller stdio closure combination and close-on-exec ownership of capture descriptors. */
static void closed_stdio(void) {
    int mask = atoi(fern_arg(2)); report_fd = fcntl(1,F_DUPFD_CLOEXEC,10); REQUIRE(report_fd >= 10);
    struct stat before[3];
    for (int fd=0;fd<3;fd++) { if (mask & (1<<fd)) REQUIRE(close(fd)==0); else REQUIRE(fstat(fd,&before[fd])==0); }
    success(child("streams",NULL,1000,3),7,"out","err");
    success(child("fds",NULL,1000,0),0,"","");
    for (int fd=0;fd<3;fd++) {
        if (mask & (1<<fd)) { REQUIRE(fcntl(fd,F_GETFD)==-1); REQUIRE(errno==EBADF); }
        else { struct stat after; REQUIRE(fstat(fd,&after)==0); REQUIRE(before[fd].st_dev==after.st_dev); REQUIRE(before[fd].st_ino==after.st_ino); }
    }
}

/** Count open owned-process descriptors within a fixed test inventory. @return Count. */
static int descriptor_count(void) {
    int count=0; for(int fd=0;fd<256;fd++) if(fcntl(fd,F_GETFD)!=-1) count++;
    return count;
}

/** Repeated success and failure must release every capture descriptor. */
static void descriptors(void) {
    int before=descriptor_count();
    for(unsigned i=0;i<40;i++) {
        success(child("empty",NULL,1000,0),0,"","");
        char* argv[]={"/missing-fern-bounded"}; failure(invoke(argv,1,1000,0),2);
        failure(child("literal","x",1000,0),4);
    }
    REQUIRE(descriptor_count()==before);
}

/** Exhaust descriptor capacity at every setup stage; restore caller limits and inventory. */
static void exhausted_descriptors(void) {
    struct rlimit old, limited; REQUIRE(getrlimit(RLIMIT_NOFILE,&old)==0);
    limited=old; if(limited.rlim_cur>64) limited.rlim_cur=64;
    REQUIRE(setrlimit(RLIMIT_NOFILE,&limited)==0);
    int baseline=descriptor_count();
    for(unsigned spare=0;spare<6;spare++) {
        int owned[64],count=0,fd;
        while(count<64 && (fd=open("/dev/null",O_RDONLY|O_CLOEXEC))>=0) owned[count++]=fd;
        REQUIRE(errno==EMFILE); REQUIRE(count>(int)spare);
        for(unsigned i=0;i<spare;i++) REQUIRE(close(owned[--count])==0);
        failure(child("empty",NULL,1000,0),5);
        while(count>0) REQUIRE(close(owned[--count])==0);
        REQUIRE(descriptor_count()==baseline);
    }
    REQUIRE(setrlimit(RLIMIT_NOFILE,&old)==0);
}

/** Validate count and terminator-inclusive byte limits without assuming host ARG_MAX. */
static void argument_limits(void) {
    char* many[4097]; many[0]=(char*)self; many[1]="child"; many[2]="empty";
    for(unsigned i=3;i<4097;i++) many[i]="";
    success(invoke(many,4096,2000,0),0,"",""); failure(invoke(many,4097,2000,0),1);
    char* bytes=calloc(1024*1024+1,1); REQUIRE(bytes!=NULL);
    memset(bytes,'x',1024*1024);
    char* args[]={(char*)self,"child","empty",bytes};
    size_t used=strlen(self)+1+sizeof("child")+sizeof("empty");
    size_t length=1024*1024-used-1; bytes[length]=0;
    int64_t raw=invoke(args,4,2000,0);
    if(fern_result_is_ok(raw)) success(raw,0,"",""); else failure(raw,2);
    bytes[length]='x'; bytes[length+1]=0; failure(invoke(args,4,2000,0),1);
    free(bytes);
    char* bad[]={"\300\257"}; failure(invoke(bad,1,1000,0),1);
}

/** A tiny signal handler perturbs blocking operations without changing errno. @param signal Signal. */
static void alarm_handler(int signal) { (void)signal; }

/** Reject incompatible child-reaping policy; inherited blocking/ignored signals must be reset in children. */
static void signals(void) {
    struct sigaction old, action; memset(&action,0,sizeof(action)); sigemptyset(&action.sa_mask);
    action.sa_handler=SIG_IGN; REQUIRE(sigaction(SIGCHLD,&action,&old)==0);
    failure(child("empty",NULL,1000,0),1); REQUIRE(sigaction(SIGCHLD,&old,NULL)==0);
    action.sa_handler=SIG_DFL; action.sa_flags=SA_NOCLDWAIT; REQUIRE(sigaction(SIGCHLD,&action,&old)==0);
    failure(child("empty",NULL,1000,0),1); REQUIRE(sigaction(SIGCHLD,&old,NULL)==0);
    action.sa_handler=SIG_IGN; action.sa_flags=0; REQUIRE(sigaction(SIGTERM,&action,&old)==0);
    sigset_t mask, original; sigemptyset(&mask); sigaddset(&mask,SIGTERM); REQUIRE(sigprocmask(SIG_BLOCK,&mask,&original)==0);
    failure(child("signal",NULL,1000,0),7);
    REQUIRE(sigprocmask(SIG_SETMASK,&original,NULL)==0); REQUIRE(sigaction(SIGTERM,&old,NULL)==0);
    action.sa_handler=alarm_handler; REQUIRE(sigaction(SIGALRM,&action,&old)==0);
    struct itimerval timer={{0,1000},{0,1000}}, zero={{0,0},{0,0}};
    REQUIRE(setitimer(ITIMER_REAL,&timer,NULL)==0); failure(child("sleep",NULL,30,0),3);
    REQUIRE(setitimer(ITIMER_REAL,&zero,NULL)==0); REQUIRE(sigaction(SIGALRM,&old,NULL)==0);
}

/** Select one bounded oracle group; the existing runtime main initializes the GC and argv. @return Process status. */
int fern_main(void) {
    self=fern_arg(0); const char* mode=fern_arg(1);
    if(strcmp(mode,"child")==0) return child_main();
    if(strcmp(mode,"basic")==0) basic(); else if(strcmp(mode,"output")==0) output();
    else if(strcmp(mode,"invalid")==0) invalid(); else if(strcmp(mode,"spawn")==0) spawn_errors();
    else if(strcmp(mode,"lifecycle")==0) lifecycle(); else if(strcmp(mode,"closed")==0) closed_stdio();
    else if(strcmp(mode,"escaped")==0) escaped();
    else if(strcmp(mode,"maximum")==0) maximum_output();
    else if(strcmp(mode,"path")==0) path_search();
    else if(strcmp(mode,"exhausted")==0) exhausted_descriptors();
    else if(strcmp(mode,"arguments")==0) argument_limits();
    else if(strcmp(mode,"fds")==0) descriptors(); else if(strcmp(mode,"signals")==0) signals(); else return 99;
    dprintf(report_fd,"ok:%s\n",mode); return 0;
}
