#include "../../runtime/fern_json.c"
static const FernJsonCodec scalar={.kind=0};
static const FernJsonCodec* children[]={&scalar,&scalar};
static const FernJsonCodecVariant variants[]={{"Pair",2,children}};
static const FernJsonCodec sum={.kind=12,.count=1,.variants=variants};
/** Envelope strings, array and both payloads share the original native allowance. */
int fern_main(void) {
    int64_t data[]={0,1,2};JsonCodecState state=codec_begin();state.budget.work=500;
    if(codec_encode(&state,&sum,(int64_t)(intptr_t)data,0)!=NULL || state.failure->code!=4 || strcmp(state.failure->path,"/fields/1"))return 1;
    if(state.budget.nodes!=6 || state.budget.allocated!=1054 || state.budget.work!=111) {
        fprintf(stderr,"sum budget: %zu %zu %zu\n",state.budget.nodes,state.budget.allocated,state.budget.work);return 2;
    }
    puts("sum JSON envelope and payload budget profile passed");return 0;
}
