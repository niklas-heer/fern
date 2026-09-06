/** Stable source constructor envelopes; included after primitive codec helpers. */
_Static_assert(sizeof(FernJsonCodecVariant)==24,"sum variant descriptor ABI requires three words");
static bool codec_same_key(JsonCodecState*,const FernJsonValue*,const char*);
/** Retain a structural failure path before publishing its original code. @param c context; @param key path; @param code error. */
static void codec_sum_fail(JsonCodecState* c,const char* key,int code) {
    assert(c != NULL && key != NULL);
    const char* path=codec_path(c,key,strlen(key));
    if(path)c->path=path;
    codec_fail(c,code,-1);
}
/** Build the positional payload array without fabricating a tuple plan. @param c context; @param v variant; @param data payloads; @param depth array depth; @return JSON array. */
static FernJsonValue* codec_sum_fields(JsonCodecState* c,const FernJsonCodecVariant* v,const int64_t* data,size_t depth) {
    assert(c != NULL && v != NULL && data != NULL && v->count>=0);
    size_t count=(size_t)v->count;if(!codec_work(c,count))return NULL;
    FernJsonValue* out=json_node(&c->budget,J_ARRAY);if(!codec_ready(c))return NULL;
    out->length=count;out->children=codec_alloc(c,count*8);if(!out->children)return NULL;
    for(size_t i=0;i<count && codec_ready(c);i++) {
        char index[32];if(!codec_work(c,20) || !codec_reserve(c,21))return NULL;
        int length=snprintf(index,sizeof(index),"%zu",i);assert(length>0 && length<32);
        out->children[i]=codec_encode_at(c,v->children[i],data[i],index,(size_t)length,depth+1);
    }
    if(!codec_ready(c))return NULL;
    return json_seal(&c->budget,out) && codec_ready(c)?out:NULL;
}
/** Encode exact tag/fields key order and source names under the original shared allowance. @param c context; @param p sum; @param bits native storage; @param depth current; @return object. */
static FernJsonValue* codec_encode_sum(JsonCodecState* c,const FernJsonCodec* p,int64_t bits,size_t depth) {
    assert(c != NULL && p != NULL && bits != 0);
    const int64_t* data=(const void*)(intptr_t)bits;
    if(data[0]<0 || data[0]>=p->count) {codec_sum_fail(c,"tag",13);return NULL;}
    const FernJsonCodecVariant* v=&p->variants[data[0]];
    if(!codec_work(c,4))return NULL;
    FernJsonValue* out=json_node(&c->budget,J_OBJECT);if(!codec_ready(c))return NULL;
    out->length=4;out->children=codec_alloc(c,32);if(!out->children)return NULL;
    out->children[0]=codec_text(c,"tag");if(!codec_ready(c))return NULL;
    const char* old=c->path;size_t old_length=c->path_length;
    const char* path=codec_path(c,"tag",3);if(!path)return NULL;c->path=path;
    if(!codec_step(c,p,depth+1))return NULL;
    out->children[1]=codec_text(c,v->name);c->path=old;c->path_length=old_length;
    if(!codec_ready(c))return NULL;
    out->children[2]=codec_text(c,"fields");if(!codec_ready(c))return NULL;
    path=codec_path(c,"fields",6);if(!path)return NULL;c->path=path;
    if(!codec_step(c,p,depth+1))return NULL;
    out->children[3]=codec_sum_fields(c,v,data+1,depth+1);
    c->path=old;c->path_length=old_length;if(!codec_ready(c))return NULL;
    return json_seal(&c->budget,out) && codec_ready(c)?out:NULL;
}
/** Check all input keys before canonical missing fields, without constructing payload candidates. @param c context; @param v object; @param tag output; @param fields output; @return valid. */
static bool codec_sum_envelope(JsonCodecState* c,const FernJsonValue* v,const FernJsonValue** tag,const FernJsonValue** fields) {
    assert(c != NULL && v != NULL && tag != NULL && fields != NULL);
    if(v->kind!=J_OBJECT) {codec_fail(c,5,-1);return false;}
    for(size_t i=0;i<v->length && codec_ready(c);i++) {
        const FernJsonValue* key=v->children[i*2];
        if(!codec_work(c,key->length))return false;
        if(memchr(key->text,0,key->length)) {codec_fail(c,10,-1);return false;}
        bool is_tag=codec_same_key(c,key,"tag"), is_fields=codec_same_key(c,key,"fields");
        if(!codec_ready(c))return false;
        if(is_tag)*tag=v->children[i*2+1];
        else if(is_fields)*fields=v->children[i*2+1];
        else {
            const char* path=codec_path(c,key->text,key->length);if(path)c->path=path;
            codec_fail(c,12,-1);return false;
        }
    }
    if(!*tag)codec_sum_fail(c,"tag",6);
    else if(!*fields)codec_sum_fail(c,"fields",6);
    return codec_ready(c);
}
/** Resolve one stable source name with fully charged comparisons. @param c context; @param p descriptor; @param tag JSON; @return native tag or -1 on failure. */
static int64_t codec_sum_tag(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* tag) {
    assert(c != NULL && p != NULL && tag != NULL);
    if(tag->kind!=J_STRING) {codec_fail(c,5,-1);return -1;}
    for(int64_t i=0;i<p->count && codec_ready(c);i++) {
        if(codec_same_key(c,tag,p->variants[i].name))return i;
    }
    codec_fail(c,13,-1);return -1;
}
/** Convert one selected constructor after complete envelope/tag/array validation. @param c context; @param v variant; @param fields JSON array; @param tag ordinal; @param depth array depth; @return native storage bits. */
static int64_t codec_sum_payload(JsonCodecState* c,const FernJsonCodecVariant* v,const FernJsonValue* fields,int64_t tag,size_t depth) {
    assert(c != NULL && v != NULL && fields != NULL);
    if(fields->kind!=J_ARRAY || fields->length!=(size_t)v->count) {codec_fail(c,5,-1);return 0;}
    int64_t* out=codec_slots(c,fields->length,false);if(!out)return 0;out[0]=tag;
    for(size_t i=0;i<fields->length && codec_ready(c);i++) {
        char index[32];if(!codec_work(c,20) || !codec_reserve(c,21))return 0;
        int length=snprintf(index,sizeof(index),"%zu",i);assert(length>0 && length<32);
        out[i+1]=codec_decode_at(c,v->children[i],fields->children[i],index,(size_t)length,depth+1);
    }
    return codec_ready(c)?(int64_t)(intptr_t)out:0;
}
/** Decode exactly one sum envelope without resetting budgets or conversion failures. @param c context; @param p plan; @param v JSON; @param depth current; @return native storage. */
static int64_t codec_decode_sum(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    const FernJsonValue *tag=NULL,*fields=NULL;
    if(!codec_sum_envelope(c,v,&tag,&fields))return 0;
    const char* old=c->path;size_t old_length=c->path_length;
    const char* path=codec_path(c,"tag",3);if(!path)return 0;c->path=path;
    if(!codec_step(c,p,depth+1))return 0;
    int64_t index=codec_sum_tag(c,p,tag);if(!codec_ready(c))return 0;
    c->path=old;c->path_length=old_length;
    path=codec_path(c,"fields",6);if(!path)return 0;c->path=path;
    if(!codec_step(c,p,depth+1))return 0;
    int64_t result=codec_sum_payload(c,&p->variants[index],fields,index,depth+1);
    c->path=old;c->path_length=old_length;return result;
}
