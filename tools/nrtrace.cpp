#include "nr/nr.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

void usage() {
  nr::out("nrtrace - network path trace (simulated)");
  nr::out();
  nr::out("  usage  nrtrace <host> [--fast] [--no-color] [--seed <n>] [--dialog]");
  nr::out();
  nr::out("  hops and latencies are generated locally.");
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string target;
  bool show_dialog = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (arg == "--dialog") {
      show_dialog = true;
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (arg == "--seed" && i + 1 < argc) {
      try {
        nr::seed_rng(static_cast<std::uint64_t>(std::stoull(argv[++i])));
      } catch (...) {
        std::cerr << "invalid seed\n";
        return 2;
      }
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    } else if (target.empty()) {
      target = arg;
    }
  }
  if (target.empty()) {
    usage();
    return 2;
  }

  const int hops = 5 + nr::rnd(5);
  int lost = 0;
  double worst = 0.0;

  nr::head("nrtrace " + target);
  nr::out();

  for (int hop = 1; hop <= hops; ++hop) {
    bool last = hop == hops;
    std::string number = nr::pad_left(std::to_string(hop), 3);

    if (!last && nr::rnd(100) < 12) {
      ++lost;
      nr::out("  " + number + "  *");
    } else {
      std::string address;
      if (hop == 1) {
        address = "10.0.0.1";
      } else if (hop == 2) {
        address = "172.16." + std::to_string(nr::rnd(200) + 1) + ".1";
      } else if (last) {
        address = target;
      } else {
        address = nr::fake_ip(target + ":" + std::to_string(hop));
      }

      double ms = 0.4 + hop * 0.9 + nr::rnd(300) / 100.0;
      worst = std::max(worst, ms);
      nr::out("  " + number + "  " + nr::pad_right(address, 20) +
              nr::pad_left(nr::fixed(ms, 2), 7) + " ms");
    }
    nr::sleep_ms(120);
  }

  int loss = lost * 100 / hops;
  std::string outcome = std::to_string(hops) + " hops - " +
                        std::to_string(loss) + "% loss - " +
                        nr::fixed(worst, 2) + " ms peak";
  nr::out();
  nr::summary(outcome);
  if (show_dialog && nr::tty()) {
    nr::dialog(outcome + "\n\nNo traffic left this machine.", "nrtrace",
               nr::Icon::info);
  }
  return 0;
}
