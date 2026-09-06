/** Bounded logical graph accounting; source values remain shared on the existing GC heap. */
#define MANAGED_GRAPH 4096u
#define MANAGED_DEPTH 128u

typedef struct { const void* pointer; const FernManagedType* type; bool active; } ManagedSeen;
typedef struct {
    ManagedSession* session;
    ManagedSeen* seen;
    size_t count, work, bytes;
} ManagedCost;

/** Validate finite descriptor metadata before callbacks or payload reads. @param root Type. @param nullable Absent effect allowed. @return Valid shape. */
static bool managed_descriptor_walk(const FernManagedType* root,bool nullable,size_t* work) {
    if(++*work>MANAGED_WORK) return false;
    if(root==NULL) return nullable;
    const FernManagedType* pending[MANAGED_GRAPH]; const FernManagedType* seen[MANAGED_GRAPH];
    size_t count=0,used=1; pending[0]=root;
    while(used) {
        const FernManagedType* type=pending[--used]; bool known=false;
        if(type==NULL || ++*work>MANAGED_WORK) return false;
        for(size_t i=0;i<count;i++) {
            if(++*work>MANAGED_WORK) return false;
            if(seen[i]==type) { known=true; break; }
        }
        if(known) continue;
        if(count>=MANAGED_GRAPH || type->kind<0 || type->kind>FERN_MANAGED_UNACCOUNTED || type->count<0 || type->count>4096) return false;
        seen[count++]=type;
        size_t children=(size_t)type->count;
        if(type->kind==FERN_MANAGED_LIST || type->kind==FERN_MANAGED_UNBOXED || type->kind==FERN_MANAGED_PID) { if(children!=1) return false; }
        else if(type->kind==FERN_MANAGED_MAP) { if(children!=2) return false; }
        else if(type->kind==FERN_MANAGED_SUM) {
            if(children==0 || !type->arities) return false;
            size_t total=0;
            for(size_t i=0;i<children;i++) { if(++*work>MANAGED_WORK) return false; if(type->arities[i]<0 || type->arities[i]>4096 || total>4096-(size_t)type->arities[i]) return false; total+=(size_t)type->arities[i]; }
            children=total;
        } else if(type->kind!=FERN_MANAGED_PRODUCT && children!=0) return false;
        if(children>MANAGED_GRAPH-used || (children && !type->children)) return false;
        for(size_t i=0;i<children;i++) {
            if(++*work>MANAGED_WORK) return false;
            pending[used++]=type->children[i];
        }
    }
    return true;
}

/** Validate one immutable descriptor under a fresh bounded operation allowance. @param root Type. @param nullable Accept no type. @return Valid graph. */
static bool managed_descriptor(const FernManagedType* root,bool nullable) {
    size_t work=0; return managed_descriptor_walk(root,nullable,&work);
}

/** Bound bytes and traversal work before payload descent. @param cost Accumulator. @param bytes Additional logical bytes. @return Fits. */
static bool managed_cost_add(ManagedCost* cost,size_t bytes) {
    assert(cost!=NULL);
    if(++cost->work>MANAGED_WORK || bytes>MANAGED_BYTES-cost->bytes) return false;
    cost->bytes+=bytes; return true;
}

/** Detect active cycles while sharing already-finished immutable subgraphs. @param cost Owner. @param type Shape. @param pointer Heap identity. @return New index, -1 shared, -2 invalid. */
static int managed_seen(ManagedCost* cost,const FernManagedType* type,const void* pointer) {
    assert(cost!=NULL);
    if(!pointer) return -2;
    for(size_t i=0;i<cost->count;i++) {
        if(++cost->work>MANAGED_WORK) return -2;
        if(cost->seen[i].pointer==pointer && cost->seen[i].type==type) return cost->seen[i].active ? -2 : -1;
    }
    if(cost->count>=MANAGED_GRAPH) return -2;
    size_t i=cost->count++; cost->seen[i]=(ManagedSeen){pointer,type,true}; return (int)i;
}
static bool managed_cost_value(ManagedCost*,const FernManagedType*,int64_t,unsigned);

/** Follow captures only through a known compiler descriptor. @param cost Owner. @param closure Code word plus payloads. @param depth Nesting. @return Bounded graph. */
static bool managed_cost_frame(ManagedCost* cost,const void* closure,unsigned depth) {
    assert(cost!=NULL);
    if(depth>=MANAGED_DEPTH) return false;
    const FernManagedFunction* function=managed_function_work(cost->session,closure,&cost->work);
    if(!function || !managed_cost_add(cost,8u*(1u+(size_t)function->capture_count))) return false;
    const int64_t* values=closure;
    for(int64_t i=0;i<function->capture_count;i++) if(!managed_cost_value(cost,function->captures[i],values[i+1],depth+1)) return false;
    return true;
}

/** Account list backing capacity and each live payload before enqueue. @param cost Owner. @param type List/map shape. @param list Value. @param depth Nesting. @return Bounded graph. */
static bool managed_cost_list(ManagedCost* cost,const FernManagedType* type,const FernList* list,unsigned depth) {
    assert(cost!=NULL && type!=NULL);
    if(list->len<0 || list->cap<1 || list->len>list->cap || (uint64_t)list->cap>MANAGED_BYTES/8 || !list->data) return false;
    if(!managed_cost_add(cost,24u+8u*(size_t)list->cap)) return false;
    for(int64_t i=0;i<list->len;i++) {
        if(type->kind==FERN_MANAGED_MAP) {
            const int64_t* pair=(const int64_t*)(intptr_t)list->data[i];
            if(!pair || pair[0]!=0 || !managed_cost_add(cost,24)) return false;
            if(!managed_cost_value(cost,type->children[0],pair[1],depth+1) || !managed_cost_value(cost,type->children[1],pair[2],depth+1)) return false;
        } else if(!managed_cost_value(cost,type->children[0],list->data[i],depth+1)) return false;
    }
    return true;
}

/** Validate the tag before projecting product/sum payloads. @param cost Owner. @param type Shape. @param fields Tagged value. @param depth Nesting. @return Bounded graph. */
static bool managed_cost_fields(ManagedCost* cost,const FernManagedType* type,const int64_t* fields,unsigned depth) {
    assert(cost!=NULL && type!=NULL);
    int64_t tag=fields[0]; size_t first=0,count=(size_t)type->count;
    if(type->kind==FERN_MANAGED_SUM) {
        if(tag<0 || tag>=type->count) return false;
        for(int64_t i=0;i<tag;i++) {
            if(++cost->work>MANAGED_WORK) return false;
            first+=(size_t)type->arities[i];
        }
        count=(size_t)type->arities[tag];
    } else if(tag!=0) return false;
    if(!managed_cost_add(cost,8u*(count+1u))) return false;
    for(size_t i=0;i<count;i++) if(!managed_cost_value(cost,type->children[first+i],fields[i+1],depth+1)) return false;
    return true;
}

/** Scan at most16MiB, charging each64-byte unit before reads and respecting remaining byte capacity.
 * @param cost Shared attempt budget. @param text Existing native String. @return NUL-terminated text fits.
 */
static bool managed_cost_string(ManagedCost* cost,const unsigned char* text) {
    assert(cost!=NULL && text!=NULL);
    size_t limit=16u*1024u*1024u+1u;
    if(limit>MANAGED_BYTES-cost->bytes) limit=MANAGED_BYTES-cost->bytes;
    for(size_t length=0;length<limit;length++) {
        if(length%64u==0 && ++cost->work>MANAGED_WORK) return false;
        if(text[length]==0) return managed_cost_add(cost,length+1u);
    }
    return false;
}

/** Account one full-width value after its semantic descriptor is validated. @param cost Owner. @param type Shape. @param value Payload. @param depth Nesting. @return Fits. */
static bool managed_cost_value(ManagedCost* cost,const FernManagedType* type,int64_t value,unsigned depth) {
    assert(cost!=NULL && type!=NULL);
    if(depth>=MANAGED_DEPTH || ++cost->work>MANAGED_WORK) return false;
    if(type->kind==FERN_MANAGED_SCALAR) return managed_cost_add(cost,8);
    if(type->kind==FERN_MANAGED_UNBOXED) return managed_cost_value(cost,type->children[0],value,depth+1);
    const void* pointer=(const void*)(intptr_t)value;
    int index=managed_seen(cost,type,pointer); if(index==-1) return true; if(index<0) return false;
    bool valid=false;
    switch(type->kind) {
    case FERN_MANAGED_STRING: valid=managed_cost_string(cost,pointer); break;
    case FERN_MANAGED_LIST: case FERN_MANAGED_MAP: valid=managed_cost_list(cost,type,pointer,depth); break;
    case FERN_MANAGED_PRODUCT: case FERN_MANAGED_SUM: valid=managed_cost_fields(cost,type,pointer,depth); break;
    case FERN_MANAGED_PID: {
        const ManagedPid* pid=pointer; ManagedSession* s=cost->session;
        valid=pid->session==s && pid->id>0 && pid->id<=s->next_id && s->identities[pid->id-1]==pid->actor
            && pid->mailbox==type->children[0] && managed_cost_add(cost,sizeof(ManagedPid)); break;
    }
    case FERN_MANAGED_RANGE: valid=managed_cost_add(cost,24); break;
    case FERN_MANAGED_FUNCTION: valid=managed_cost_frame(cost,pointer,depth); break;
    /* Immutable JSON needs its own audited retained-size bridge before it is publishable. */
    case FERN_MANAGED_JSON: valid=false; break;
    default: break;
    }
    cost->seen[index].active=false; return valid;
}

/** Allocate a bounded temporary graph index, reclaimed on every success/failure. @param s Owner. @param type Shape or NULL for frame. @param value Payload. @param bytes Output. @return Accounted. */
static bool managed_cost(ManagedSession* s,const FernManagedType* type,int64_t value,size_t* bytes) {
    assert(s!=NULL && bytes!=NULL);
    ManagedSeen* seen=calloc(MANAGED_GRAPH,sizeof(*seen)); if(!seen) return false;
    ManagedCost cost={s,seen,0,0,0};
    bool valid=type ? managed_descriptor_walk(type,false,&cost.work) && managed_cost_value(&cost,type,value,0) : managed_cost_frame(&cost,(void*)(intptr_t)value,0);
    if(valid) *bytes=cost.bytes;
    memset(seen,0,MANAGED_GRAPH*sizeof(*seen)); free(seen); return valid;
}

/** Charge one candidate frame graph. @param s Owner. @param frame Closure. @param bytes Output. @return Valid bounded frame. */
static bool managed_frame_cost(ManagedSession* s,const void* frame,size_t* bytes) {
    return managed_cost(s,NULL,(int64_t)(intptr_t)frame,bytes);
}

/** Charge one message graph independently of other queue owners. @param s Owner. @param type Shape. @param value Payload. @param bytes Output. @return Valid bounded message. */
static bool managed_value_cost(ManagedSession* s,const FernManagedType* type,int64_t value,size_t* bytes) {
    return managed_cost(s,type,value,bytes);
}
