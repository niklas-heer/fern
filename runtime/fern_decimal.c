/** Unicode16 decimal predicate and bounded text preflight; see Decision92. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "fern_runtime.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fern_decimal_table.h"
#define DECIMAL_LIMIT (16u * 1024u * 1024u)

/** Bound scanning independently of character classification, without allocating.
 * @param text Native CString, or NULL handled defensively as false by the predicate.
 * @return 1 within the content ceiling, 0 if oversized. NULL passes size preflight.
 */
int64_t fern_str_decimal_size_is_valid(const char* text){
    assert(DECIMAL_LIMIT<SIZE_MAX);
    assert(sizeof(decimal_ranges)/sizeof(*decimal_ranges)==71);
    return text==NULL || strnlen(text,DECIMAL_LIMIT+1)<=DECIMAL_LIMIT;
}

/** Decode one strict scalar, consuming only available bytes.
 * @param text Non-null bytes. @param length Available bytes. @param scalar Scalar output.
 * @return Consumed width1..4, or0 for malformed/truncated input.
 */
static size_t decimal_decode(const char* text,size_t length,uint32_t* scalar){
    assert(text!=NULL && scalar!=NULL);
    assert(length>0 && length<=DECIMAL_LIMIT);
    unsigned char a=(unsigned char)text[0];
    size_t width=a<0x80?1:a>=0xc2&&a<=0xdf?2:a>=0xe0&&a<=0xef?3:a>=0xf0&&a<=0xf4?4:0;
    if(width==0 || width>length)return 0;
    uint32_t value=width==1?a:a&((1u<<(7-width))-1u);
    for(size_t i=1;i<width;i++){
        unsigned char b=(unsigned char)text[i];
        if(b<0x80 || b>0xbf)return 0;
        value=(value<<6)|(b&63);
    }
    if((width==2&&value<0x80)||(width==3&&value<0x800)||(width==4&&value<0x10000))return 0;
    if(value>0x10ffff || (value>=0xd800&&value<=0xdfff))return 0;
    *scalar=value;return width;
}

/** Search the generated disjoint Nd intervals with a fixed comparison ceiling.
 * @param scalar Valid Unicode scalar. @return True exactly for pinned Nd membership.
 */
static bool decimal_member(uint32_t scalar){
    assert(scalar<=0x10ffff);
    assert(scalar<0xd800 || scalar>0xdfff);
    if(scalar<0x80)return scalar>='0'&&scalar<='9';
    size_t low=0,high=sizeof(decimal_ranges)/sizeof(*decimal_ranges);
    for(unsigned step=0;step<7 && low<high;step++){
        size_t middle=low+(high-low)/2;
        if(scalar<decimal_ranges[middle][0])high=middle;
        else if(scalar>decimal_ranges[middle][1])low=middle+1;
        else return true;
    }
    return false;
}

/** Classify nonempty strict UTF8 text as entirely Unicode16 Nd digits.
 * @param text Native CString; NULL/malformed text yields false. Content cap16MiB.
 * @return Canonical native Bool0/1. Oversize is a runtime fault, never false.
 */
int64_t fern_str_is_decimal(const char* text){
    if(text==NULL)return 0;
    size_t length=strnlen(text,DECIMAL_LIMIT+1);
    if(length>DECIMAL_LIMIT){
        fputs("fern: runtime error: string size limit exceeded\n",stderr);exit(1);
    }
    assert(length<=DECIMAL_LIMIT);
    assert(sizeof(decimal_ranges)/sizeof(*decimal_ranges)==71);
    if(length==0)return 0;
    for(size_t offset=0;offset<length;){
        uint32_t scalar=0;
        size_t width=decimal_decode(text+offset,length-offset,&scalar);
        if(width==0 || !decimal_member(scalar))return 0;
        offset+=width;
    }
    return 1;
}
