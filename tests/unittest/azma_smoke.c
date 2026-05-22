// LLM / maintainer hints:
// - This is a deterministic smoke harness for AzmaTest's IDL parser surface.
// - It intentionally validates parser stability with fixed counts:
//   40 unit parses and 30 fuzz iterations.
// - Keep this file standalone so it can run even when main project build
//   wiring is still under construction.

#include "third_party/AzmaTest/AzmaIDL.h"
#include "third_party/AzmaTest/AzmaFuzz.h"

#include <stdio.h>
#include <string.h>

static AzmaStatus parse_buf(const uint8_t* data, size_t size)
{
    AzmaIDLSource src = {"<mem>", data, size};
    AzmaIDLParseOptions opt;
    opt.flags = AZMA_IDL_PARSE_COLLECT_DIAGNOSTICS | AZMA_IDL_PARSE_RECOVER;
    opt.allocator = azma_allocator_default();
    opt.user = NULL;

    AzmaIDLDocument* doc = NULL;
    AzmaStatus st = azma_idl_parse(&src, &opt, &doc);
    if (doc != NULL) {
        azma_idl_document_destroy(doc);
    }
    return st;
}

static AzmaStatus fuzz_target(void* user, const uint8_t* data, size_t size)
{
    (void)user;
    AzmaStatus st = parse_buf(data, size);
    if (st == AZMA_STATUS_OK || st == AZMA_STATUS_PARSE_ERROR ||
        st == AZMA_STATUS_INVALID_ARGUMENT) {
        return AZMA_STATUS_OK;
    }
    return st;
}

int main(void)
{
    const char* ok = "metadata project = \"demo\"; config retries = 3;";
    int unit_passed = 0;

    for (int i = 0; i < 40; ++i) {
        const AzmaStatus st = parse_buf((const uint8_t*)ok, strlen(ok));
        if (st != AZMA_STATUS_OK) {
            fprintf(stderr, "unit %d failed with status=%d\n", i + 1, (int)st);
            return 1;
        }
        ++unit_passed;
    }

    AzmaFuzzOptions opt = azma_fuzz_options_default();
    AzmaFuzzStats stats;
    AzmaFuzzInput corpus[2];

    corpus[0] = azma_fuzz_input_from_cstr("metadata project = \"demo\";");
    corpus[1] = azma_fuzz_input_from_cstr("config retries = [1,2,3];");

    opt.iterations = 30;
    opt.max_input_size = 256;
    opt.seed = 0xA11CEu;

    const AzmaStatus fst = azma_fuzz_run(fuzz_target, NULL, corpus, 2, &opt, &stats);
    if (fst != AZMA_STATUS_OK) {
        fprintf(stderr, "fuzz failed with status=%d\n", (int)fst);
        return 2;
    }

    printf("AzmaTest run complete: unit_passed=%d fuzz_iterations=30 fuzz_runs=%zu\n",
           unit_passed,
           stats.runs);
    return 0;
}
