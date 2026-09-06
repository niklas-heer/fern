/** Shallow predicates borrow DOM keys; they never allocate paths, payloads or candidate Results. */
/** Match only stable known tags for an all-sum object union. @param c context; @param p sum; @param v object; @return known tag. */
static bool codec_union_tag(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v) {
    assert(c != NULL && p != NULL && v != NULL && v->kind==J_OBJECT);
    for(size_t i=0;i<v->length && codec_ready(c);i++) {
        if(!codec_same_key(c,v->children[i*2],"tag"))continue;
        const FernJsonValue* tag=v->children[i*2+1];if(tag->kind!=J_STRING)return false;
        bool found=false;
        for(int64_t j=0;j<p->count && codec_ready(c);j++)found|=codec_same_key(c,tag,p->variants[j].name);
        return found;
    }
    return false;
}
/** Recognize exact envelope keys for mixed record/sum unions. @param c context; @param v object; @return exact tag/fields key set. */
static bool codec_union_sum_keys(JsonCodecState* c,const FernJsonValue* v) {
    assert(c != NULL && v != NULL && v->kind==J_OBJECT);
    bool tag=false,fields=false;
    for(size_t i=0;i<v->length && codec_ready(c);i++) {
        bool a=codec_same_key(c,v->children[i*2],"tag"),b=codec_same_key(c,v->children[i*2],"fields");
        if(!a && !b)return false;
        tag|=a;fields|=b;
    }
    return tag && fields;
}
/** Require all actual keys allowed and all actual non-Option fields present. @param c context; @param p record; @param v object; @return strict key profile. */
static bool codec_union_record_keys(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v) {
    assert(c != NULL && p != NULL && v != NULL && v->kind==J_OBJECT);
    for(size_t i=0;i<v->length && codec_ready(c);i++) {
        bool known=false;
        for(int64_t j=0;j<p->count && codec_ready(c);j++)known|=codec_same_key(c,v->children[i*2],p->names[j]);
        if(!known)return false;
    }
    for(int64_t j=0;j<p->count && codec_ready(c);j++) {
        if(!codec_work(c,1))return false;
        if(p->children[j]->kind==7)continue;
        bool found=false;
        for(size_t i=0;i<v->length && codec_ready(c);i++)found|=codec_same_key(c,v->children[i*2],p->names[j]);
        if(!found)return false;
    }
    return codec_ready(c);
}
/** Follow only transparent edges, then apply a key/length/tag predicate. @param c context; @param p plan; @param v DOM; @param tags all-sum policy; @param depth metadata depth; @return candidate shape. */
static bool codec_union_shape(JsonCodecState* c,const FernJsonCodec* p,const FernJsonValue* v,bool tags,size_t depth) {
    assert(c != NULL && p != NULL && v != NULL);
    if(!codec_step(c,p,depth))return false;
    if(p->kind==11)return codec_union_shape(c,p->children[0],v,tags,depth+1);
    if(p->kind==7)return v->kind==J_NULL || codec_union_shape(c,p->children[0],v,tags,depth+1);
    if(p->kind==13) {
        bool found=false;
        for(int64_t i=0;i<p->count && codec_ready(c);i++)found|=codec_union_shape(c,p->children[i],v,tags,depth+1);
        return found;
    }
    if(p->kind==12)return v->kind==J_OBJECT && (tags?codec_union_tag(c,p,v):codec_union_sum_keys(c,v));
    if(p->kind==10)return v->kind==J_OBJECT && codec_union_record_keys(c,p,v);
    if(p->kind==8)return v->kind==J_ARRAY && v->length==(size_t)p->count;
    return (codec_union_profile(c,p,depth+1,false)&codec_union_bit(v))!=0;
}
