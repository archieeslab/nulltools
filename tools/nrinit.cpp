#include "nr/nr.hpp"

#include <iostream>
#include <string>

namespace {

struct Step {
  const char* key;
  const char* value;
  const char* body;
  nr::Icon icon;
};

const Step kSteps[] = {
    {"runtime", "accepted", "Runtime initialized.\n\nbuild 1.0.0 / x64", nr::Icon::info},
    {"signatures", "214 days old",
     "Signature database is 214 days old.\n\nUse the cached rules for this session?",
     nr::Icon::warning},
    {"engine", "attached", "Capture engine attached.\n\n4 interfaces available.",
     nr::Icon::info},
    {"workspace", "/root/.nullroute",
     "Workspace prepared.\n\nNo files were written to disk.", nr::Icon::info},
};

void usage() {
  nr::out("nrinit - environment bootstrap (simulated)");
  nr::out();
  nr::out("  usage  nrinit [--no-dialogs] [--fast] [--no-color]");
  nr::out();
  nr::out("  opens native confirmation dialogs for each boot step.");
  nr::out("  dialogs are skipped automatically when output is not a terminal.");
}

}

int main(int argc, char** argv) {
  nr::init();
  bool dialogs = true;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--no-dialogs") {
      dialogs = false;
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    }
  }

  if (!nr::tty()) dialogs = false;

  nr::head("nrinit");
  nr::out();

  for (const Step& step : kSteps) {
    nr::out("  " + nr::pad_right(step.key, 12) + step.value);
    nr::sleep_ms(240);
    if (!dialogs) continue;

    if (!nr::dialog(step.body, "nullroute setup", step.icon)) {
      nr::out("  " + nr::dim("operator      cancelled"));
      return 1;
    }
    nr::out("  " + nr::dim("operator      confirmed"));
  }

  nr::out();
  nr::kv("environment", "ready", 12);
  nr::out();
  nr::kv("next", "nrscan 10.0.0.14", 12);

  if (dialogs) {
    nr::dialog(
        "Environment ready.\n\nnrscan, nrtrace, nrcrack, nrdump and nrwatch "
        "are available.",
        "nullroute setup", nr::Icon::info);
  }

  return 0;
}
