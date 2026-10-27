#include "nr/nr.hpp"

#include <iostream>
#include <string>

namespace {

void usage() {
  nr::out("nrcrack - key recovery (simulated)");
  nr::out();
  nr::out("  usage  nrcrack [file] [--fast] [--no-color] [--seed <n>] [--dialog]");
  nr::out();
  nr::out("  no file is opened. the key and plaintext are invented.");
}

std::string clock_str(int seconds) {
  int minutes = seconds / 60;
  int rest = seconds % 60;
  return (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
         (rest < 10 ? "0" : "") + std::to_string(rest);
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string file;
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
    } else if (file.empty()) {
      file = arg;
    }
  }
  if (file.empty()) file = "creds.enc";

  nr::head("nrcrack " + file);
  nr::out();
  nr::kv("cipher", "aes-256-cbc");
  nr::kv("wordlist", "rockyou.txt (" + nr::num(14344391) + ")");
  nr::kv("workers", "12");
  nr::out();

  double rate = 1.8 + nr::rnd(14) / 10.0;
  long long tested = 0;
  int elapsed = 0;

  if (nr::tty() && !nr::fast()) {
    for (int i = 0; i < 46; ++i) {
      tested += static_cast<long long>(rate * 55000.0);
      elapsed += 1;
      std::cout << "\r  " << nr::dim("tested") << "  "
                << nr::pad_left(nr::num(tested), 10) << "  " << nr::dim("-")
                << "  " << nr::fixed(rate, 1) << " M/s" << "  " << nr::dim("-")
                << "  " << clock_str(elapsed / 4) << "\x1b[K" << std::flush;
      nr::sleep_ms(55);
    }
    std::cout << "\r\x1b[K";
  } else {
    tested = 14344391LL;
    elapsed = 2 + nr::rnd(3);
    nr::out("  " + nr::dim("tested") + "  " + nr::pad_left(nr::num(tested), 10) +
            "  " + nr::dim("-") + "  " + nr::fixed(rate, 1) + " M/s" + "  " +
            nr::dim("-") + "  " + clock_str(elapsed));
  }

  nr::out();
  nr::kv("match", "hunter2");
  nr::kv("key", nr::hexs(64));
  nr::out();

  nr::out("  " + nr::strong("credentials"));
  nr::kv("admin", "hunter2", 10);
  nr::kv("db_svc", nr::hexs(24), 10);
  nr::kv("vpn", nr::hexs(32), 10);

  nr::out();
  std::string outcome = nr::num(tested) + " candidates - " + clock_str(elapsed) +
                        " - match";
  nr::summary(outcome);
  if (show_dialog && nr::tty()) {
    nr::dialog("Key recovered.\n\n" + outcome + "\n\nNo file was opened.",
               "nrcrack", nr::Icon::info);
  }
  return 0;
}
