/** Independent native scalar/text oracles for Unicode16 decimal classification. */
#include "fern_runtime.h"
#include "fern_gc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int64_t fern_str_is_decimal(const char* text);
extern int64_t fern_str_decimal_size_is_valid(const char* text);
#define LIMIT (16u * 1024u * 1024u)
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"failed line%d\n",__LINE__); exit(90); } } while(0)

/** Encode a known scalar into a five-byte test buffer. */
static void encode(uint32_t c,char* out){
    if(c<0x80){out[0]=(char)c;out[1]=0;}
    else if(c<0x800){out[0]=(char)(0xc0|(c>>6));out[1]=(char)(0x80|(c&63));out[2]=0;}
    else if(c<0x10000){out[0]=(char)(0xe0|(c>>12));out[1]=(char)(0x80|((c>>6)&63));out[2]=(char)(0x80|(c&63));out[3]=0;}
    else{out[0]=(char)(0xf0|(c>>18));out[1]=(char)(0x80|((c>>12)&63));out[2]=(char)(0x80|((c>>6)&63));out[3]=(char)(0x80|(c&63));out[4]=0;}
}

/** Emit one classification byte for every scalar/code point for the primary-data oracle. */
static void exhaustive(void){
    unsigned char output[4096];size_t used=0;
    for(uint32_t c=0;c<0x110000;c++){
        char text[5];encode(c,text);
        output[used++]=(unsigned char)fern_str_is_decimal(text);
        if(used==sizeof(output)){REQUIRE(fwrite(output,1,used,stdout)==used);used=0;}
    }
    if(used)REQUIRE(fwrite(output,1,used,stdout)==used);
}

/** Verify text semantics including foreign malformed UTF8 without crashing. */
static void texts(void){
    const char* yes[]={"0","0123456789","١٢٣","۱۲۳","१२३","１２３","𝟘𝟙𝟚","0١２𝟛"};
    const char* no[]={"","-1","1.2","1e2"," 1","1\n","²","①","Ⅳ","½","1́", "\x80", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "1\xf0\x9f"};
    for(size_t i=0;i<sizeof(yes)/sizeof(*yes);i++)REQUIRE(fern_str_is_decimal(yes[i])==1);
    for(size_t i=0;i<sizeof(no)/sizeof(*no);i++)REQUIRE(fern_str_is_decimal(no[i])==0);
    REQUIRE(fern_str_is_decimal(NULL)==0);
}

/** Drive exactly one bounded oracle group. */
int main(int argc,char** argv){
    REQUIRE(argc==2);fern_gc_init();
    if(strcmp(argv[1],"all")==0){exhaustive();return 0;}
    if(strcmp(argv[1],"text")==0){texts();puts("ok");return 0;}
    char* text=FERN_ALLOC(LIMIT+2);REQUIRE(text!=NULL);memset(text,'1',LIMIT+1);text[LIMIT+1]=0;
    REQUIRE(fern_str_decimal_size_is_valid(text)==0);
    if(strcmp(argv[1],"oversize")==0)return (int)fern_str_is_decimal(text);
    if(strcmp(argv[1],"oversize_nondecimal")==0){text[0]='x';return (int)fern_str_is_decimal(text);}
    REQUIRE(strcmp(argv[1],"limit")==0);text[LIMIT]=0;
    REQUIRE(fern_str_decimal_size_is_valid(text)==1);REQUIRE(fern_str_is_decimal(text)==1);
    text[LIMIT-1]='x';REQUIRE(fern_str_is_decimal(text)==0);puts("ok");return 0;
}
