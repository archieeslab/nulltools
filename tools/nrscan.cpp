#include "nr/nr.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Service {
  int port;
  bool open;
  const char* name;
  const char* version;
};

const Service kPool[] = {
    {22, true, "ssh", "OpenSSH 8.9p1 Ubuntu 3ubuntu0.6"},
    {80, true, "http", "nginx 1.24.0"},
    {443, true, "https", "nginx 1.24.0"},
    {3306, true, "mysql", "MySQL 8.0.34"},
    {5432, true, "postgresql", "PostgreSQL 15.4"},
    {6379, true, "redis", "Redis 7.0.11"},
    {8080, true, "http-proxy", "Squid 5.7"},
    {9001, true, "tor-orport", "Tor 0.4.8.10"},
    {27017, true, "mongodb", "MongoDB 6.0.8"},
    {21, false, "ftp", "-"},
    {3389, false, "ms-wbt-server", "-"},
};

constexpr int kOpen = 9;

void usage() {
  nr::out("nrscan - port scan (simulated)");
  nr::out();
  nr::out("  usage  nrscan <host> [--fast] [--no-color] [--seed <n>] [--dialog]");
  nr::out();
  nr::out("  every port, banner and version below is generated locally.");
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

  std::vector<int> open_idx(kOpen);
  for (int i = 0; i < kOpen; ++i) open_idx[static_cast<std::size_t>(i)] = i;
  for (std::size_t i = open_idx.size(); i > 1; --i) {
    std::swap(open_idx[i - 1],
              open_idx[static_cast<std::size_t>(nr::rnd(static_cast<int>(i)))]);
  }

  int open_count = 4 + nr::rnd(4);
  int filtered_count = nr::rnd(3);

  std::vector<Service> services;
  for (int i = 0; i < open_count; ++i) {
    services.push_back(
        kPool[static_cast<std::size_t>(open_idx[static_cast<std::size_t>(i)])]);
  }
  for (int i = 0; i < filtered_count; ++i) {
    services.push_back(kPool[static_cast<std::size_t>(kOpen + nr::rnd(2))]);
  }
  std::sort(services.begin(), services.end(),
            [](const Service& a, const Service& b) { return a.port < b.port; });

  nr::Table table({"port", "state", "service", "version"});
  for (const auto& service : services) {
    table.add({std::to_string(service.port),
               service.open ? "open" : "filtered", service.name,
               service.version});
  }

  nr::head("nrscan " + target);
  nr::out();
  nr::kv("target", target);
  nr::kv("reverse", nr::fake_host(target));
  nr::kv("profile", "syn - top 1000");
  nr::out();

  table.print_header();
  double started = nr::now_ms();
  for (std::size_t i = 0; i < table.size(); ++i) {
    nr::sleep_ms(70);
    table.print_row(i);
  }
  double elapsed = (nr::now_ms() - started) / 1000.0;
  if (nr::fast()) elapsed = 0.62 + nr::rnd(70) / 100.0;

  int closed = 1000 - open_count - filtered_count;
  std::string outcome = nr::num(open_count) + " open - " + nr::num(closed) +
                        " closed - " + nr::num(filtered_count) + " filtered - " +
                        nr::fixed(elapsed, 2) + "s";

  nr::out();
  nr::summary(outcome);
  if (show_dialog && nr::tty()) {
    nr::dialog(outcome + "\n\nNo packets were sent.", "nrscan", nr::Icon::info);
  }
  return 0;
}
