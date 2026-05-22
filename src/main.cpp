// LLM / maintainer hints:
// - Keep this entrypoint focused on initialization/lifecycle orchestration.
// - Delegate argument handling to src/olnk/cli.cpp; do not duplicate CLI logic here.
// - The public C ABI lifecycle must stay deterministic and exception-free.

#include <olnk/olnk-api.h>

namespace olnk {
int run_cli(int argc, char** argv);
}  // namespace olnk

int main(int argc, char** argv) {
  if (olnk_initialize() != OLNK_STATUS_OK) {
    return 1;
  }

  const int cli_status = olnk::run_cli(argc, argv);

  olnk_shutdown();
  return cli_status;
}
