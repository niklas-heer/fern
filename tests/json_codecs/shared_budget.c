#include "../../runtime/fern_json.c"
static const FernJsonCodec scalar={0,0,NULL,NULL};
static const FernJsonCodec* children[]={&scalar};
static const FernJsonCodec sequence={6,1,children,NULL};
/** Siblings share the same allowance; exhaustion occurs before the second primitive adapter. */
int fern_main(void) {
    int64_t data[]={1,2};FernList values={.data=data,.len=2,.cap=2};
    JsonCodecState state=codec_begin();state.budget.work=500;
    if(codec_encode(&state,&sequence,(int64_t)(intptr_t)&values,0)!=NULL || state.failure->code!=4 || strcmp(state.failure->path,"/1")) {
        fputs("sibling work allowance reset or incorrect failure path\n",stderr);return 1;
    }
    if(state.budget.nodes!=2 || state.budget.allocated!=602 || state.budget.work!=193) {
        fputs("codec budget counters differ from the shared interpreter profile\n",stderr);return 1;
    }
    state=codec_begin();state.path="/original";state.path_length=9;codec_fail(&state,9,17);state.budget.work=0;
    if(codec_path(&state,"child",5)!=NULL || state.failure->code!=9 || state.failure->offset!=17 || strcmp(state.failure->path,"/original")) {
        fputs("path growth replaced the original error\n",stderr);return 1;
    }
    puts("typed JSON shared-budget cases passed");return 0;
}
