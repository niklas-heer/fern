/** Concrete typed codec interpreter; included in the JSON translation unit to reuse primitive adapters. */
_Static_assert(sizeof(FernJsonCodec) == 32, "codec descriptor ABI requires four 64-bit words");
typedef struct {
    JsonParser budget;
    FernJsonError* failure;
    int64_t* result;
    const char* path;
    size_t path_length;
} JsonCodecState;

/** Retain the first failure and its already reserved location. @param c context; @param code stable code; @param offset primitive offset. */
static void codec_fail(JsonCodecState* c, int code, int64_t offset) {
    assert(c != NULL && code >= 1 && code <= 12);
    if (!c->failure->code) { c->failure->code=code; c->failure->offset=offset; c->failure->path=c->path; }
}
/** Transfer deterministic allocation/work failure without replacing an earlier conversion failure. @param c context; @return still valid. */
static bool codec_ready(JsonCodecState* c) {
    assert(c != NULL);
    if (c->budget.code) codec_fail(c,c->budget.code,-1);
    return !c->failure->code;
}
/** Charge bounded work before any byte scan or comparison. @param c context; @param n units; @return available. */
static bool codec_work(JsonCodecState* c,size_t n) {
    assert(c != NULL);
    if (!codec_ready(c)) return false;
    (void)json_work(&c->budget,n);
    return codec_ready(c);
}
/** Charge and allocate zeroed managed storage. @param c context; @param n bytes; @return storage or NULL. */
static void* codec_alloc(JsonCodecState* c,size_t n) {
    assert(c != NULL);
    if (!codec_ready(c)) return NULL;
    void* value=json_allocate(&c->budget,n);
    return codec_ready(c)?value:NULL;
}
/** Reserve existing primitive adapter allocations before calling them. @param c context; @param n bytes; @return available. */
static bool codec_reserve(JsonCodecState* c,size_t n) {
    assert(c != NULL && c->budget.allocated <= JSON_ALLOC_MAX);
    if (!codec_ready(c)) return false;
    if (n > JSON_ALLOC_MAX-c->budget.allocated) { codec_fail(c,4,-1); return false; }
    c->budget.allocated+=n;
    return true;
}
/** Scan a source String with charges before each byte read. @param c context; @param text source; @param length output; @return within limit. */
static bool codec_length(JsonCodecState* c,const char* text,size_t* length) {
    assert(c != NULL && text != NULL && length != NULL);
    for (size_t i=0;i<=JSON_INPUT_MAX;i++) {
        if (!codec_work(c,1)) return false;
        if (!text[i]) { *length=i; return true; }
    }
    codec_fail(c,4,-1);return false;
}
/** Extend an immutable JSON Pointer before descent; allocation failure remains at the parent path. @param c context; @param key bytes; @param length count; @return new path or NULL. */
static const char* codec_path(JsonCodecState* c,const char* key,size_t length) {
    assert(c != NULL && key != NULL && length <= JSON_INPUT_MAX);
    if (c->path_length >= JSON_OUTPUT_MAX || length > (JSON_OUTPUT_MAX-c->path_length-1)/2) { codec_fail(c,4,-1);return NULL; }
    size_t maximum=c->path_length+1+2*length;
    if (!codec_work(c,maximum) || !codec_reserve(c,40)) return NULL;
    char* path=codec_alloc(c,maximum+1);if (!path) return NULL;
    memcpy(path,c->path,c->path_length);size_t at=c->path_length;path[at++]='/';
    for (size_t i=0;i<length;i++) {
        if (key[i]=='~' || key[i]=='/') {path[at++]='~';path[at++]=key[i]=='~'?'0':'1';}
        else path[at++]=key[i];
    }
    c->path_length=at;return path;
}
/** Validate recursion/work before reading an already checked descriptor. @param c context; @param plan descriptor; @param depth current depth; @return available. */
static bool codec_step(JsonCodecState* c,const FernJsonCodec* plan,size_t depth) {
    assert(c != NULL && plan != NULL && plan->kind>=0 && plan->kind<=11 && plan->count>=0);
    (void)plan;
    if (depth>=JSON_DEPTH_MAX) {codec_fail(c,4,-1);return false;}
    return codec_work(c,1);
}
/** Preserve the original code/offset from an existing primitive adapter. @param c context; @param result heap Result; @return success payload bits. */
static int64_t codec_primitive_result(JsonCodecState* c,int64_t result) {
    assert(c != NULL && result != 0);
    int64_t payload=fern_result_unwrap(result);
    if (!fern_result_is_ok(result)) {
        const FernJsonError* error=(const void*)(intptr_t)payload;
        codec_fail(c,(int)error->code,error->offset);return 0;
    }
    return payload;
}
static FernJsonValue* codec_encode(JsonCodecState*,const FernJsonCodec*,int64_t,size_t);
static int64_t codec_decode(JsonCodecState*,const FernJsonCodec*,const FernJsonValue*,size_t);

/** Decode one child at a retained field/index path. @param c context; @param plan descriptor; @param value JSON child; @param key path segment; @param n segment bytes; @param depth next depth; @return bits. */
static int64_t codec_decode_at(JsonCodecState* c,const FernJsonCodec* plan,const FernJsonValue* value,const char* key,size_t n,size_t depth) {
    assert(c != NULL && plan != NULL && value != NULL);
    const char* old=c->path;size_t old_length=c->path_length;
    const char* path=codec_path(c,key,n);if (!path) return 0;
    c->path=path;int64_t result=codec_decode(c,plan,value,depth);
    c->path=old;c->path_length=old_length;return result;
}
/** Encode one child while retaining its failure path. @param c context; @param plan descriptor; @param value bits; @param key segment; @param n bytes; @param depth next depth; @return JSON child. */
static FernJsonValue* codec_encode_at(JsonCodecState* c,const FernJsonCodec* plan,int64_t value,const char* key,size_t n,size_t depth) {
    assert(c != NULL && plan != NULL && key != NULL);
    const char* old=c->path;size_t old_length=c->path_length;
    const char* path=codec_path(c,key,n);if (!path) return NULL;
    c->path=path;FernJsonValue* result=codec_encode(c,plan,value,depth);
    c->path=old;c->path_length=old_length;return result;
}
/** Build text with the existing validated UTF-8 adapter and shared budget. @param c context; @param text source String; @return JSON string. */
static FernJsonValue* codec_text(JsonCodecState* c,const char* text) {
    assert(c != NULL && text != NULL);
    size_t length;if (!codec_length(c,text,&length) || !codec_work(c,length*2)) return NULL;
    FernJsonValue* value=json_text_node(&c->budget,text,length);
    return codec_ready(c)?value:NULL;
}
/** Reuse exact integer/Float primitive adapters after charging their bounded work/storage. @param c context; @param kind primitive kind; @param bits source bits; @return JSON scalar. */
static FernJsonValue* codec_scalar(JsonCodecState* c,int64_t kind,int64_t bits) {
    assert(c != NULL && kind>=0 && kind<=5);
    if (!codec_work(c,256) || !codec_reserve(c,256)) return NULL;
    if (c->budget.nodes==JSON_NODES_MAX) {codec_fail(c,4,-1);return NULL;}
    c->budget.nodes++;
    switch (kind) {
        case 0:return fern_json_value_from_int(bits);
        case 1:{double value;memcpy(&value,&bits,8);return (void*)(intptr_t)codec_primitive_result(c,fern_json_value_from_float(value));}
        case 2:return fern_json_value_from_bool(bits);
        case 4:return fern_json_value_null();
        default:codec_fail(c,5,-1);return NULL;
    }
}
/** Allocate native tagged/list storage only after charging the full required slot count. @param c context; @param count payload slots; @param list whether list header; @return storage. */
static int64_t* codec_slots(JsonCodecState* c,size_t count,bool list) {
    assert(c != NULL);
    if (count>JSON_NODES_MAX || !codec_work(c,count)) {codec_fail(c,4,-1);return NULL;}
    return codec_alloc(c,(count+(list?3:1))*8);
}

/** Encode a list/tuple with source-order children. @param c context; @param p descriptor; @param bits storage; @param depth current depth; @return sealed array. */
static FernJsonValue* codec_encode_array(JsonCodecState* c,const FernJsonCodec* p,int64_t bits,size_t depth) {
    assert(c != NULL && p != NULL && bits != 0);
    const int64_t* data;size_t count;
    if (p->kind==6) {const FernList* list=(const void*)(intptr_t)bits;assert(list->len>=0);count=(size_t)list->len;data=list->data;}
    else {count=(size_t)p->count;data=(const int64_t*)(intptr_t)bits+1;}
    if (count>JSON_NODES_MAX || !codec_work(c,count)) {codec_fail(c,4,-1);return NULL;}
    FernJsonValue* out=json_node(&c->budget,J_ARRAY);if (!codec_ready(c)) return NULL;
    out->length=count;out->children=codec_alloc(c,count*8);if (!out->children) return NULL;
    for (size_t i=0;i<count && codec_ready(c);i++) {
        char index[32];if (!codec_work(c,20) || !codec_reserve(c,21)) return NULL;
        int length=snprintf(index,sizeof(index),"%zu",i);assert(length>0 && length<32);
        out->children[i]=codec_encode_at(c,p->children[p->kind==6?0:i],data[i],index,(size_t)length,depth+1);
    }
    if (!codec_ready(c)) return NULL;
    return json_seal(&c->budget,out) && codec_ready(c)?out:NULL;
}
/** Encode ordered record slots or String-key map pairs. @param c context; @param p descriptor; @param bits storage; @param depth current depth; @return sealed object. */
static FernJsonValue* codec_encode_object(JsonCodecState* c,const FernJsonCodec* p,int64_t bits,size_t depth) {
    assert(c != NULL && p != NULL && bits != 0);
    const FernList* map=p->kind==9?(const void*)(intptr_t)bits:NULL;
    size_t count=map?(size_t)map->len:(size_t)p->count;
    if (count>JSON_NODES_MAX/2 || !codec_work(c,count*2)) {codec_fail(c,4,-1);return NULL;}
    FernJsonValue* out=json_node(&c->budget,J_OBJECT);if (!codec_ready(c)) return NULL;
    out->length=count*2;out->children=codec_alloc(c,count*16);if (!out->children) return NULL;
    for (size_t i=0;i<count && codec_ready(c);i++) {
        const int64_t* pair=map?(const void*)(intptr_t)map->data[i]:NULL;
        const char* key=map?(const void*)(intptr_t)pair[0]:p->names[i];
        int64_t value=map?pair[1]:((const int64_t*)(intptr_t)bits)[i+1];
        out->children[i*2]=codec_text(c,key);if (!codec_ready(c)) return NULL;
        out->children[i*2+1]=codec_encode_at(c,p->children[map?0:i],value,key,out->children[i*2]->length,depth+1);
    }
    if (!codec_ready(c)) return NULL;
    return json_seal(&c->budget,out) && codec_ready(c)?out:NULL;
}
/** Dispatch a concrete value without a type guess or representation coercion. @param c context; @param p checked plan; @param bits payload; @param depth depth; @return JSON value. */
static FernJsonValue* codec_encode(JsonCodecState* c,const FernJsonCodec* p,int64_t bits,size_t depth) {
    if (!codec_step(c,p,depth)) return NULL;
    switch(p->kind) {
        case 0:case 1:case 2:case 4:return codec_scalar(c,p->kind,bits);
        case 3:return codec_text(c,(const void*)(intptr_t)bits);
        case 5:{FernJsonValue* value=(void*)(intptr_t)bits;return codec_work(c,value->nodes)?value:NULL;}
        case 6:case 8:return codec_encode_array(c,p,bits,depth);
        case 7:{const int64_t* sum=(const void*)(intptr_t)bits;return sum[0]?codec_scalar(c,4,0):codec_encode(c,p->children[0],sum[1],depth+1);}
        case 9:case 10:return codec_encode_object(c,p,bits,depth);
        case 11:return codec_encode(c,p->children[0],bits,depth+1);
        default:codec_fail(c,5,-1);return NULL;
    }
}
/** Decode array fields after exact tuple/minimum list shape validation. @param c context; @param p plan; @param v JSON; @param depth depth; @return native storage bits. */
static int64_t codec_decode_array(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    if (v->kind!=J_ARRAY || (p->kind==8 && v->length!=(size_t)p->count)) {codec_fail(c,5,-1);return 0;}
    bool list=p->kind==6;int64_t* memory=codec_slots(c,v->length,list);if (!memory) return 0;
    int64_t* data=memory+(list?3:1);
    if (list) {FernList* out=(void*)memory;out->data=data;out->len=(int64_t)v->length;out->cap=out->len;}
    for (size_t i=0;i<v->length && codec_ready(c);i++) {
        char index[32];if (!codec_work(c,20) || !codec_reserve(c,21)) return 0;
        int n=snprintf(index,sizeof(index),"%zu",i);assert(n>0 && n<32);
        data[i]=codec_decode_at(c,p->children[list?0:i],v->children[i],index,(size_t)n,depth+1);
    }
    return codec_ready(c)?(int64_t)(intptr_t)memory:0;
}
/** Compare exact decoded key bytes after charging the whole possible comparison. @param c context; @param key JSON String; @param name source field; @return equal. */
static bool codec_same_key(JsonCodecState* c,const FernJsonValue* key,const char* name) {
    assert(c != NULL && key != NULL && key->kind==J_STRING && name != NULL);
    size_t length;if (!codec_length(c,name,&length) || !codec_work(c,(length<key->length?length:key->length)+1)) return false;
    return length==key->length && !memcmp(key->text,name,length);
}
/** Validate all record keys before required-field conversion. @param c context; @param p plan; @param v object; @return all keys admitted. */
static bool codec_known_fields(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v) {
    assert(c != NULL && p != NULL && v != NULL && v->kind==J_OBJECT);
    for (size_t i=0;i<v->length && codec_ready(c);i++) {
        const FernJsonValue* key=v->children[i*2];
        if (!codec_work(c,key->length)) return false;
        if (memchr(key->text,0,key->length)) {codec_fail(c,10,-1);return false;}
        bool known=false;
        for (size_t j=0;j<(size_t)p->count && codec_ready(c);j++) known=codec_same_key(c,key,p->names[j])||known;
        if (!codec_ready(c)) return false;
        if (!known) {
            const char* path=codec_path(c,key->text,key->length);if (path)c->path=path;
            codec_fail(c,12,-1);return false;
        }
    }
    return codec_ready(c);
}
/** Create an existing heap Option representation under the shared budget. @param c context; @param tag None1/Some0; @param payload full-width payload; @return pointer bits. */
static int64_t codec_option(JsonCodecState* c,int64_t tag,int64_t payload) {
    assert(c != NULL && (tag==0 || tag==1));
    int64_t* result=codec_slots(c,1,false);if (!result) return 0;
    result[0]=tag;result[1]=payload;return (int64_t)(intptr_t)result;
}
/** Decode source record fields in declaration order, treating only nullable-safe Option as optional. @param c context; @param p plan; @param v object; @param depth depth; @return tagged storage. */
static int64_t codec_decode_record(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    if (v->kind!=J_OBJECT) {codec_fail(c,5,-1);return 0;}
    if (!codec_known_fields(c,p,v)) return 0;
    int64_t* fields=codec_slots(c,(size_t)p->count,false);if (!fields) return 0;
    for (size_t i=0;i<(size_t)p->count && codec_ready(c);i++) {
        const FernJsonValue* found=NULL;
        for (size_t j=0;j<v->length && codec_ready(c);j++) {
            if (codec_same_key(c,v->children[j*2],p->names[i])) {found=v->children[j*2+1];break;}
        }
        size_t length;if (!codec_length(c,p->names[i],&length)) return 0;
        if (found) fields[i+1]=codec_decode_at(c,p->children[i],found,p->names[i],length,depth+1);
        else {
            const char* old=c->path;size_t old_length=c->path_length;
            const char* path=codec_path(c,p->names[i],length);if(!path)return 0;c->path=path;
            if (p->children[i]->kind==7) fields[i+1]=codec_option(c,1,0);else codec_fail(c,6,-1);
            c->path=old;c->path_length=old_length;
        }
    }
    return codec_ready(c)?(int64_t)(intptr_t)fields:0;
}
/** Preserve map insertion order and exact String keys without adapter-local budgets. @param c context; @param p plan; @param v object; @param depth depth; @return native map pointer. */
static int64_t codec_decode_map(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    if (v->kind!=J_OBJECT) {codec_fail(c,5,-1);return 0;}
    FernList* map=(void*)codec_slots(c,v->length,true);if (!map)return 0;
    map->len=(int64_t)v->length;map->cap=map->len;map->data=(int64_t*)(map+1);
    for(size_t i=0;i<v->length && codec_ready(c);i++) {
        const FernJsonValue* key=v->children[i*2];if (!codec_work(c,key->length) || !codec_reserve(c,64))return 0;
        int64_t text=codec_primitive_result(c,fern_json_value_as_string(key));if (!codec_ready(c))return 0;
        int64_t* pair=codec_alloc(c,16);if (!pair)return 0;
        pair[0]=text;pair[1]=codec_decode_at(c,p->children[0],v->children[i*2+1],key->text,key->length,depth+1);
        map->data[i]=(int64_t)(intptr_t)pair;
    }
    return codec_ready(c)?(int64_t)(intptr_t)map:0;
}
/** Reuse primitive conversion Results and full-width payload bits. @param c context; @param p plan; @param v value; @param depth depth; @return decoded payload. */
static int64_t codec_decode(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,size_t depth) {
    if (!codec_step(c,p,depth))return 0;
    if (p->kind<=3) {
        size_t work=(v->kind==J_NUMBER || v->kind==J_STRING)?v->length*2:1;
        if (!codec_work(c,work) || !codec_reserve(c,64))return 0;
        switch(p->kind) {
            case 0:return codec_primitive_result(c,fern_json_value_as_int(v));
            case 1:return codec_primitive_result(c,fern_json_value_as_float(v));
            case 2:return codec_primitive_result(c,fern_json_value_as_bool(v));
            default:return codec_primitive_result(c,fern_json_value_as_string(v));
        }
    }
    switch(p->kind) {
        case 4:if (v->kind!=J_NULL)codec_fail(c,5,-1);return 0;
        case 5:return codec_work(c,v->nodes)?(int64_t)(intptr_t)v:0;
        case 6:case 8:return codec_decode_array(c,p,v,depth);
        case 7:{
            int64_t* option=codec_slots(c,1,false);if(!option)return 0;
            option[0]=v->kind==J_NULL?1:0;
            if(!option[0])option[1]=codec_decode(c,p->children[0],v,depth+1);
            return codec_ready(c)?(int64_t)(intptr_t)option:0;
        }
        case 9:return codec_decode_map(c,p,v,depth);
        case 10:return codec_decode_record(c,p,v,depth);
        case 11:return codec_decode(c,p->children[0],v,depth+1);
        default:codec_fail(c,5,-1);return 0;
    }
}
/** Reserve final success/error publication before traversal. @return initialized context. */
static JsonCodecState codec_begin(void) {
    JsonCodecState c={.budget={.work=67108864,.allocated=128},.path=""};
    c.failure=fern_alloc(sizeof(*c.failure));memset(c.failure,0,sizeof(*c.failure));
    c.result=fern_alloc(16);c.failure->offset=-1;c.failure->path="";return c;
}
/** Publish the already reserved heap Result without replacing error code, offset or path. @param c context; @param payload success bits; @return Result. */
static int64_t codec_finish(JsonCodecState* c,int64_t payload) {
    assert(c != NULL && c->result != NULL && c->failure != NULL);
    (void)codec_ready(c);c->result[0]=c->failure->code?1:0;
    c->result[1]=c->failure->code?(int64_t)(intptr_t)c->failure:payload;return (int64_t)(intptr_t)c->result;
}
/** Return the immutable pointer path already retained by an error. @param error valid error; @return path. */
const char* fern_json_value_error_path(const FernJsonError* error) {
    assert(error != NULL && error->path != NULL);return error->path;
}
/** Encode a checked payload and stringify once under one shared allowance. @param plan checked plan; @param payload bits; @return Result(String,Error). */
int64_t fern_json_codec_encode(const FernJsonCodec* plan,int64_t payload) {
    assert(plan != NULL);JsonCodecState c=codec_begin();
    FernJsonValue* value=codec_encode(&c,plan,payload,0);if (!codec_ready(&c))return codec_finish(&c,0);
    if (value->encoded>JSON_OUTPUT_MAX || !codec_work(&c,value->encoded+value->nodes)) {codec_fail(&c,4,-1);return codec_finish(&c,0);}
    char* text=codec_alloc(&c,value->encoded+1);if(!text)return codec_finish(&c,0);
    JsonWriter writer={.data=text,.capacity=value->encoded};json_write_value(value,&writer,1);
    assert(writer.position==value->encoded);text[writer.position]=0;return codec_finish(&c,(int64_t)(intptr_t)text);
}
/** Parse and decode without resetting any child or phase allowance. @param plan checked plan; @param text source text; @return Result(payload,Error). */
int64_t fern_json_codec_decode(const FernJsonCodec* plan,const char* text) {
    assert(plan != NULL && text != NULL);JsonCodecState c=codec_begin();size_t length;
    if(!codec_length(&c,text,&length)) {if(c.failure->code==4)c.failure->offset=JSON_INPUT_MAX;return codec_finish(&c,0);}
    c.budget.text=(const unsigned char*)text;c.budget.length=length;
    if (!codec_work(&c,8*length))return codec_finish(&c,0);
    if(length>=3 && !memcmp(text,"\357\273\277",3))c.budget.position=3;
    FernJsonValue* value=json_value(&c.budget,1);json_space(&c.budget);
    if(!c.budget.code && c.budget.position!=length)json_fail(&c.budget,1,c.budget.position);
    if(c.budget.code) {codec_fail(&c,c.budget.code,(int64_t)c.budget.error_offset);return codec_finish(&c,0);}
    c.budget.position=0;c.budget.length=0;
    int64_t decoded=codec_decode(&c,plan,value,0);return codec_finish(&c,decoded);
}
