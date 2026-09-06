/** Decoder-free shallow union selection. All metadata scans share the execution allowance. */
/** Inspect only transparent plan edges. @param c context; @param p descriptor; @param depth metadata depth; @param sums sum-object profile instead of kinds; @return mask or zero on failure. */
static unsigned codec_union_profile(JsonCodecState* c,const FernJsonCodec* p,size_t depth,bool sums) {
    assert(c != NULL && p != NULL);
    if(!codec_step(c,p,depth))return 0;
    if(p->kind==11)return codec_union_profile(c,p->children[0],depth+1,sums);
    if(p->kind==7)return (sums?0:1)|codec_union_profile(c,p->children[0],depth+1,sums);
    if(p->kind==13) {
        unsigned mask=0;
        for(int64_t i=0;i<p->count && codec_ready(c);i++)mask|=codec_union_profile(c,p->children[i],depth+1,sums);
        return mask;
    }
    if(sums)return p->kind==12?1:(p->kind==9 || p->kind==10 || p->kind==5)?2:0;
    switch(p->kind) {
        case 0:case 1:return 4;
        case 2:return 2;case 3:return 8;case 4:return 1;case 5:return 63;
        case 6:case 8:return 16;
        case 9:case 10:case 12:return 32;
        default:return 0;
    }
}
/** Convert the validated JSON kind to a shallow bit. @param v DOM value; @return nonzero bit. */
static unsigned codec_union_bit(const FernJsonValue* v) {
    assert(v != NULL);
    assert(v->kind>=J_NULL && v->kind<=J_OBJECT);
    return 1u<<(unsigned)v->kind;
}
#include "fern_json_codec_union_shapes.inc"
/** Select one child without decoding it or allocating storage. @param c shared context; @param p union; @param v DOM; @return child ordinal or -1. */
static int64_t codec_union_select(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v) {
    assert(c != NULL && p != NULL && p->kind==13 && v != NULL);
    unsigned bit=codec_union_bit(v);int64_t count=0,selected=-1;
    for(int64_t i=0;i<p->count && codec_ready(c);i++) {
        if(codec_union_profile(c,p->children[i],0,false)&bit) {count++;selected=i;}
    }
    if(!codec_ready(c))return -1;
    if(count==1)return selected;
    if(count==0) {codec_fail(c,14,-1);return -1;}
    bool sums=bit==32;
    if(sums) {
        for(int64_t i=0;i<p->count && codec_ready(c);i++) {
            if((codec_union_profile(c,p->children[i],0,false)&bit) && codec_union_profile(c,p->children[i],0,true)!=1)sums=false;
        }
    }
    count=0;
    for(int64_t i=0;i<p->count && codec_ready(c);i++) {
        if((codec_union_profile(c,p->children[i],0,false)&bit) && codec_union_shape(c,p->children[i],v,sums,0)) {count++;selected=i;}
    }
    if(!codec_ready(c))return -1;
    if(count==1)return selected;
    codec_fail(c,14,-1);return -1;
}
/** Encode the stored member directly, preserving its full payload word. @param c context; @param p union; @param bits native union; @param depth data depth; @return JSON or failure. */
static FernJsonValue* codec_encode_union(JsonCodecState* c,const FernJsonCodec* p,int64_t bits,size_t depth) {
    assert(c != NULL && p != NULL && bits != 0);
    const int64_t* data=(const void*)(intptr_t)bits;
    if(data[0]<0 || data[0]>=p->count) {codec_fail(c,14,-1);return NULL;}
    return codec_encode(c,p->children[data[0]],data[1],depth+1);
}
/** Allocate an envelope only after the selected payload succeeds. @param c context; @param p union; @param v DOM; @param depth data depth; @return native bits or failure. */
static int64_t codec_decode_union(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    int64_t selected=codec_union_select(c,p,v);if(selected<0)return 0;
    int64_t payload=codec_decode(c,p->children[selected],v,depth+1);if(!codec_ready(c))return 0;
    int64_t* out=codec_slots(c,1,false);if(!out)return 0;
    out[0]=selected;out[1]=payload;return (int64_t)(intptr_t)out;
}
