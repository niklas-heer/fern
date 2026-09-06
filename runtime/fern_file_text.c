/** Complete UTF8 file-text I/O; Decision88 preserves Result signatures and stable error codes. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "fern_runtime.h"
#include "fern_gc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define FILE_TEXT_LIMIT (16u * 1024u * 1024u)

/** Validate every byte before publishing a String. @param text Bytes. @param length Bounded length. @return NUL-free strict UTF8. */
static bool file_text_valid(const char* text,size_t length){
    assert(text!=NULL);
    assert(length<=FILE_TEXT_LIMIT);
    for(size_t i=0;i<length;){
        unsigned char a=(unsigned char)text[i++];
        if(a==0)return false;
        if(a<0x80)continue;
        unsigned extra=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:0;
        if(extra==0||extra>length-i)return false;
        unsigned char b=(unsigned char)text[i];
        if((a==0xe0&&b<0xa0)||(a==0xed&&b>=0xa0)||(a==0xf0&&b<0x90)||(a==0xf4&&b>=0x90))return false;
        for(unsigned j=0;j<extra;j++){unsigned char byte=(unsigned char)text[i++];if(byte<0x80||byte>0xbf)return false;}
    }
    return true;
}

/** Close exactly once, preserving any earlier failure. @param file Owned stream. @param error Prior code. @return Final error. */
static int file_text_close(FILE* file,int error){
    assert(file!=NULL);
    assert(error>=0 && error<=FERN_ERR_OUT_OF_MEMORY);
    int closed=fclose(file);
    return error!=0?error:closed==0?0:FERN_ERR_IO;
}

/** Bound seekable file length before allocating. @param file Open stream. @param length Output byte count. @return Error code. */
static int file_text_length(FILE* file,size_t* length){
    assert(file!=NULL);
    assert(length!=NULL);
    if(fseek(file,0,SEEK_END)!=0)return FERN_ERR_IO;
    long size=ftell(file);
    if(size<0 || (unsigned long)size>FILE_TEXT_LIMIT)return FERN_ERR_IO;
    if(fseek(file,0,SEEK_SET)!=0)return FERN_ERR_IO;
    *length=(size_t)size;
    return 0;
}

/** Read bounded bytes plus one growth probe, without publishing incomplete/error data.
 * @param file Open stream. @param length Expected bytes. @param contents Output buffer. @return Error code.
 */
static int file_text_read(FILE* file,size_t length,const char** contents){
    assert(file!=NULL && contents!=NULL);
    assert(length<=FILE_TEXT_LIMIT);
    char* bytes=FERN_ALLOC(length+1);
    if(bytes==NULL)return FERN_ERR_OUT_OF_MEMORY;
    size_t count=fread(bytes,1,length+1,file);
    if(count!=length || ferror(file))return FERN_ERR_IO;
    if(!file_text_valid(bytes,length))return FERN_ERR_IO;
    bytes[length]=0;
    *contents=bytes;
    return 0;
}

/** Read only complete bounded text; binary data is an ordinary IO error.
 * @param path Native path CString. @return Heap Result(String,Int), preserving open1/IO3/allocation4.
 */
int64_t fern_read_file(const char* path){
    if(path==NULL)return fern_result_err(FERN_ERR_IO);
    FILE* file=fopen(path,"rb");
    if(file==NULL)return fern_result_err(FERN_ERR_FILE_NOT_FOUND);
    size_t length=0;const char* contents=NULL;
    int error=file_text_length(file,&length);
    assert(length<=FILE_TEXT_LIMIT);
    if(error==0)error=file_text_read(file,length,&contents);
    error=file_text_close(file,error);
    if(error!=0)return fern_result_err(error);
    assert(contents!=NULL);
    return fern_result_ok((int64_t)(intptr_t)contents);
}

/** Validate input before opening, then require both complete fwrite and successful close.
 * @param path Target CString. @param contents Input CString. @param mode wb or ab. @return Heap Result(Int,Int).
 */
static int64_t file_text_write(const char* path,const char* contents,const char* mode){
    if(path==NULL || contents==NULL)return fern_result_err(FERN_ERR_IO);
    size_t length=strnlen(contents,FILE_TEXT_LIMIT+1);
    if(length>FILE_TEXT_LIMIT || !file_text_valid(contents,length))return fern_result_err(FERN_ERR_IO);
    assert(mode!=NULL);
    assert(length<=FILE_TEXT_LIMIT);
    FILE* file=fopen(path,mode);
    if(file==NULL)return fern_result_err(FERN_ERR_PERMISSION);
    size_t written=fwrite(contents,1,length,file);
    int error=written==length && !ferror(file)?0:FERN_ERR_IO;
    error=file_text_close(file,error);
    return error==0?fern_result_ok((int64_t)written):fern_result_err(error);
}

/** Replace a file with bounded text; errors after opening may leave a changed file.
 * @param path Target path. @param contents UTF8 CString <=16MiB. @return Heap Result(bytes written,Int).
 */
int64_t fern_write_file(const char* path,const char* contents){
    int64_t result=file_text_write(path,contents,"wb");
    assert(result!=0);
    assert(fern_result_unwrap(result)>=0 && fern_result_unwrap(result)<=FILE_TEXT_LIMIT);
    return result;
}

/** Append bounded text; partial append can precede an error and no durability is promised.
 * @param path Target path. @param contents UTF8 CString <=16MiB. @return Heap Result(bytes written,Int).
 */
int64_t fern_append_file(const char* path,const char* contents){
    int64_t result=file_text_write(path,contents,"ab");
    assert(result!=0);
    assert(fern_result_unwrap(result)>=0 && fern_result_unwrap(result)<=FILE_TEXT_LIMIT);
    return result;
}
