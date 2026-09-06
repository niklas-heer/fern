#include "../../runtime/fern_json.c"
/** Inject the exact arithmetic boundary without requesting an invalid path copy. */
int fern_main(void) {
    JsonCodecState state=codec_begin();
    state.path_length=JSON_OUTPUT_MAX;
    state.budget.allocated=JSON_ALLOC_MAX-40;
    if (codec_path(&state,"",0)!=NULL || state.failure->code!=4 || state.failure->offset!=-1 || strcmp(state.failure->path,"") || state.budget.allocated!=JSON_ALLOC_MAX-40) {
        fputs("path equality boundary did not fail before allocation\n",stderr);return 1;
    }
    puts("typed JSON path equality boundary passed");return 0;
}
