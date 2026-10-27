#include "nr/nr.hpp"

#include <csignal>
#include <ctime>
#include <iostream>
#include <string>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_sigint(int) { g_stop = 1; }

void usage() {
  nr::out("nrwatch - live event stream (simulated)");
  nr::out();
  nr::out("  usage  nrwatch [tag] [--lines <n>] [--interval <ms>]");
  nr::out("                  [--fast] [--no-color] [--seed <n>] [--dialog]");
  nr::out();
  nr::out("  events are invented. nothing is read from the system.");
}

struct Event {
  std::string source;
  std::string message;
  int level;
};

std::string random_ip() {
  return "10." + std::to_string(nr::rnd(200) + 1) + "." +
         std::to_string(nr::rnd(200) + 1) + "." +
         std::to_string(nr::rnd(240) + 2);
}

Event make_event() {
  switch (nr::rnd(8)) {
    case 0:
      return {"sshd", "accepted publickey for root from " + random_ip(), 0};
    case 1:
      return {"sshd", "failed password for invalid user admin from " + random_ip(), 1};
    case 2:
      return {"fw", "drop " + random_ip() + ":" + std::to_string(40000 + nr::rnd(20000)) +
                          " -> 10.0.0.14:22", 0};
    case 3:
      return {"tls", "handshake complete tls1.3 sni=api.internal", 0};
    case 4:
      return {"ratelimit", random_ip() + " exceeded 120 req/s", 1};
    case 5:
      return {"ids", "signature match ruleset 4021 severity=high", 2};
    case 6:
      return {"session", "closed after " + std::to_string(nr::rnd(900) + 30) + "s", 0};
    default:
      return {"dns", "query api.internal answered 10.0.0.14 ttl=60", 0};
  }
}

std::string clock_str(std::time_t value) {
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &value);
#else
  localtime_r(&value, &tm);
#endif
  char buffer[16];
  std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
  return buffer;
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string tag;
  int max_lines = 0;
  int interval = 700;
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
    } else if (arg == "--lines" && i + 1 < argc) {
      try {
        max_lines = std::stoi(argv[++i]);
      } catch (...) {
        std::cerr << "invalid line count\n";
        return 2;
      }
    } else if (arg == "--interval" && i + 1 < argc) {
      try {
        interval = std::stoi(argv[++i]);
      } catch (...) {
        std::cerr << "invalid interval\n";
        return 2;
      }
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
    } else if (tag.empty()) {
      tag = arg;
    }
  }

  std::signal(SIGINT, on_sigint);

  nr::head("nrwatch" + (tag.empty() ? std::string() : " " + tag));
  nr::out();

  std::time_t clock = std::time(nullptr);
  int count = 0;
  std::time_t started = clock;

  while (!g_stop) {
    Event event = make_event();
    clock += 1 + nr::rnd(3);

    std::string line = "  " + nr::dim(clock_str(clock)) + "  " +
                       nr::pad_right(event.source, 9) + event.message;
    if (event.level == 2) {
      nr::out(nr::err(line));
    } else if (event.level == 1) {
      nr::out(nr::warn(line));
    } else {
      nr::out(line);
    }

    ++count;
    if (max_lines > 0 && count >= max_lines) break;
    nr::sleep_ms(interval);
  }

  nr::out();
  long long seconds = static_cast<long long>(clock - started);
  int minutes = static_cast<int>(seconds / 60);
  int rest = static_cast<int>(seconds % 60);
  std::string outcome = std::to_string(count) + " events - " +
                        (minutes < 10 ? "0" : "") + std::to_string(minutes) +
                        ":" + (rest < 10 ? "0" : "") + std::to_string(rest);
  nr::summary(outcome);
  if (show_dialog && nr::tty()) {
    nr::dialog(outcome + "\n\nNo system events were read.", "nrwatch",
               nr::Icon::info);
  }
  return 0;
}
