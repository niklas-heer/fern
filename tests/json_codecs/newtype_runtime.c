#include "../../runtime/fern_json.c"
#include <gc/gc.h>
static const FernJsonCodec integer_codec={0,0,{.children=NULL},NULL};
static const FernJsonCodec float_codec={1,0,{.children=NULL},NULL};
static const FernJsonCodec* integers[]={&integer_codec};
static const FernJsonCodec integer_newtype={11,1,{.children=integers},NULL};
static const FernJsonCodec* floats[]={&float_codec};
static const FernJsonCodec float_newtype={11,1,{.children=floats},NULL};
/** Compare raw/wrapped adapter allocation and bits without counting unrelated runtime initialization. */
static int compare(const FernJsonCodec* raw,const FernJsonCodec* wrapped,int64_t bits) {
    JsonCodecState a=codec_begin(),b=codec_begin();
    size_t before=GC_get_total_bytes();
    FernJsonValue* x=codec_encode(&a,raw,bits,0);
    size_t raw_bytes=GC_get_total_bytes()-before;
    before=GC_get_total_bytes();
    FernJsonValue* y=codec_encode(&b,wrapped,bits,0);
    size_t wrapped_bytes=GC_get_total_bytes()-before;
    if (!x || !y || raw_bytes!=wrapped_bytes || a.budget.allocated!=b.budget.allocated || b.budget.work+1!=a.budget.work) return 1;
    JsonCodecState c=codec_begin(),d=codec_begin();
    before=GC_get_total_bytes();int64_t first=codec_decode(&c,raw,x,0);raw_bytes=GC_get_total_bytes()-before;
    before=GC_get_total_bytes();int64_t second=codec_decode(&d,wrapped,y,0);wrapped_bytes=GC_get_total_bytes()-before;
    return !codec_ready(&c) || !codec_ready(&d) || first!=bits || second!=bits || raw_bytes!=wrapped_bytes || c.budget.allocated!=d.budget.allocated;
}
/** Native wrappers add no representation allocation and preserve every Int/Float payload bit. */
int fern_main(void) {
    if (compare(&integer_codec,&integer_newtype,INT64_MIN) || compare(&integer_codec,&integer_newtype,INT64_MAX) || compare(&float_codec,&float_newtype,INT64_MIN) || compare(&float_codec,&float_newtype,1)) {
        fputs("newtype codec allocation or full64 payload mismatch\n",stderr);return 1;
    }
    puts("newtype JSON allocation and full64 payload passed");return 0;
}
