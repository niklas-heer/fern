#include "../../runtime/fern_json.c"
static const FernJsonCodec number={.kind=0}, text={.kind=3};
static const FernJsonCodec* numeric_children[]={&number,&text};
static const FernJsonCodec numeric_union={.kind=13,.count=2,.children=numeric_children};
static const FernJsonCodec* fields[]={&number};
static const char* a_names[]={"a"};
static const char* b_names[]={"b"};
static const FernJsonCodec a={.kind=10,.count=1,.children=fields,.names=a_names};
static const FernJsonCodec b={.kind=10,.count=1,.children=fields,.names=b_names};
static const FernJsonCodec* record_children[]={&a,&b};
static const FernJsonCodec record_union={.kind=13,.count=2,.children=record_children};
/** Selection inspects only shape metadata, never speculative adapters or allocations. */
int fern_main(void) {
    FernJsonValue fractional={.kind=J_NUMBER,.text="1.5",.length=3};
    JsonCodecState state=codec_begin();size_t allocated=state.budget.allocated,nodes=state.budget.nodes;
    if(codec_union_select(&state,&numeric_union,&fractional)!=0 || state.failure->code)return 1;
    if(state.budget.allocated!=allocated || state.budget.nodes!=nodes)return 2;
    state=codec_begin();(void)codec_decode(&state,&numeric_union,&fractional,0);
    if(state.failure->code!=9 || state.failure->offset!=-1 || strcmp(state.failure->path,""))return 3;
    FernJsonValue key={.kind=J_STRING,.text="a",.length=1};
    FernJsonValue* children[]={&key,&fractional};
    FernJsonValue object={.kind=J_OBJECT,.children=children,.length=1};
    state=codec_begin();allocated=state.budget.allocated;nodes=state.budget.nodes;
    if(codec_union_select(&state,&record_union,&object)!=0 || state.failure->code)return 4;
    if(state.budget.allocated!=allocated || state.budget.nodes!=nodes)return 5;
    state=codec_begin();state.budget.work=3;
    if(codec_union_select(&state,&record_union,&object)!=-1 || state.failure->code!=4)return 6;
    if(state.budget.allocated!=allocated || state.budget.nodes!=nodes)return 7;
    if(strcmp(state.failure->path,"") || state.failure->offset!=-1)return 8;
    puts("union selector shares work and allocates no candidate payloads");return 0;
}
