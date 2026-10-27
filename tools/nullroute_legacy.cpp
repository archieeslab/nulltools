#include "nr/deps.hpp"
#include "nr/nr.hpp"
#include "nr/ops.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Tool {
  const char* command;
  const char* label;
};

const Tool kTools[] = {
    {"init", "bootstrap sequence"},
    {"scan", "port scan with banners"},
    {"trace", "hop latency trace"},
    {"crack", "wordlist attack"},
    {"dump", "packet capture"},
    {"watch", "live event stream"},
    {"pe", "pe headers and imports"},
    {"find", "file and content search"},
    {"strings", "printable strings"},
    {"hash", "sha256 and md5"},
    {"hex", "hex dump"},
};

constexpr int kToolCount = static_cast<int>(sizeof(kTools) / sizeof(kTools[0]));

std::string trim(const std::string& text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t' ||
                         text[begin] == '\r')) {
    ++begin;
  }
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                         text[end - 1] == '\r')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::vector<std::string> split(const std::string& text) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : text) {
    if (c == ' ' || c == '\t') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) parts.push_back(current);
  return parts;
}

std::string argument(const std::vector<std::string>& args,
                     const std::string& fallback) {
  return args.empty() ? fallback : args[0];
}

void print_tools() {
  for (int i = 0; i < kToolCount; ++i) {
    std::cout << "  " << nr::dim(nr::pad_left(std::to_string(i + 1), 2)) << "  "
              << nr::pad_right(kTools[i].command, 10) << kTools[i].label << '\n';
  }
}

void print_help() {
  std::cout << "  commands\n\n";
  std::cout << "  pe <file>          pe headers, sections, imports\n";
  std::cout << "  hash <file>        sha256 and md5\n";
  std::cout << "  strings <file>     printable strings\n";
  std::cout << "  hex <file>         hex dump\n";
  std::cout << "  find <query>       walk a directory by name or content\n\n";
  std::cout << "  init               bootstrap sequence\n";
  std::cout << "  scan <host>        port scan, simulated\n";
  std::cout << "  trace <host>       hop trace, simulated\n";
  std::cout << "  crack <file>       wordlist attack, simulated\n";
  std::cout << "  dump <iface>       packet capture, simulated\n";
  std::cout << "  watch <topic>      live event stream, simulated\n\n";
  std::cout << "  tools              list the toolkit\n";
  std::cout << "  <number>           run a tool by number\n";
  std::cout << "  clear              clear the screen\n";
  std::cout << "  exit               leave\n\n";
  std::cout << "  real commands read only what you point them at.\n";
}

void show(const std::string& name, const std::vector<std::string>& args) {
  std::string output = nr::ops::run(name, args);
  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }
  std::cout << output << '\n';
}

void simulated_init() {
  std::cout << "  runtime     accepted\n";
  nr::sleep_ms(nr::fast() ? 0 : 160);
  std::cout << "  signatures  214 days old\n";
  nr::sleep_ms(nr::fast() ? 0 : 160);
  std::cout << "  engine      attached\n";
  nr::sleep_ms(nr::fast() ? 0 : 160);
  std::cout << "  workspace   " << nr::dim("%LOCALAPPDATA%\\nullroute") << '\n';
  std::cout << '\n' << "  environment ready\n";
}

void simulated_scan(const std::string& host) {
  std::cout << "  target    " << host << '\n';
  std::cout << "  profile   syn, top 1000\n\n";
  std::cout << "  port    state     service       version\n";
  const char* rows[] = {
      "  22      open      ssh           OpenSSH 8.9p1",
      "  80      open      http          nginx 1.24.0",
      "  443     open      https         nginx 1.24.0",
      "  3306    open      mysql         MySQL 8.0.34",
      "  8080    open      http-proxy    Squid 5.7",
  };
  for (const char* row : rows) {
    std::cout << row << '\n';
    nr::sleep_ms(nr::fast() ? 0 : 70);
  }
  std::cout << "\n  " << nr::dim("5 open - 995 closed - 0 filtered") << '\n';
}

void simulated_trace(const std::string& host) {
  std::cout << "  traceroute to " << host << ", 30 hops max\n\n";
  const char* names[] = {"gw-1", "core-4", "edge-9", "rtr-12", "isp-31"};
  for (int i = 0; i < 5; ++i) {
    double ms = 1.8 + i * 2.4;
    std::cout << "  " << nr::pad_left(std::to_string(i + 1), 2) << "  "
              << nr::pad_right(std::string(names[i]) + ".net", 22)
              << nr::fixed(ms, 2) << " ms\n";
    nr::sleep_ms(nr::fast() ? 0 : 90);
  }
  std::cout << "\n  " << nr::dim("path stable - 0% loss") << '\n';
}

void simulated_crack(const std::string& file) {
  std::cout << "  target      " << file << '\n';
  std::cout << "  cipher      aes-256-cbc\n";
  std::cout << "  wordlist    98,341 entries\n\n";
  for (int i = 0; i < 4; ++i) {
    std::cout << "  [*] trying " << nr::hexs(12) << " ...\n";
    nr::sleep_ms(nr::fast() ? 0 : 180);
  }
  std::cout << "  [*] trying " << nr::hexs(12) << " ... " << nr::ok("MATCH")
            << '\n';
  std::cout << "\n  key         " << nr::hexs(40) << '\n';
  std::cout << "  plaintext   admin:hunter2\n";
  std::cout << "  " << nr::dim("no file was modified") << '\n';
}

void simulated_dump(const std::string& iface) {
  std::cout << "  interface   " << iface << '\n';
  std::cout << "  capture     6 packets - 2.4 KB\n\n";
  const char* rows[] = {
      "  0000  45 00 00 3c 1c 46 40 00  40 06 b1 e6 0a 00 00 0e  E..<.F@.@.......",
      "  0010  ac 10 00 09 c3 50 01 bb  8b 2f 1a 03 00 00 00 00  .....P.../......",
      "  0020  a0 02 72 10 5b 8e 00 00  02 04 05 b4 04 02 08 0a  ..r.[...........",
      "  0030  00 1c 4f 2e 00 00 00 00  01 03 03 07              ..O.........",
  };
  for (const char* row : rows) {
    std::cout << nr::dim(row) << '\n';
    nr::sleep_ms(nr::fast() ? 0 : 90);
  }
  std::cout << "\n  " << nr::dim("6 packets captured - 0 dropped") << '\n';
}

void simulated_watch(const std::string& topic) {
  std::cout << "  filter      " << topic << '\n';
  std::cout << "  stream      local events only\n\n";
  const char* rows[] = {
      "[00:00:01] conn     10.0.0.14:443   ->  172.16.0.9:51820",
      "[00:00:02] dns      relay-71.corp   ->  10.0.0.1",
      "[00:00:03] auth     session opened  operator",
      "[00:00:04] file     read            README.md",
      "[00:00:05] conn     session closed  idle 30s",
  };
  for (const char* row : rows) {
    std::cout << nr::dim(row) << '\n';
    nr::sleep_ms(nr::fast() ? 0 : 140);
  }
  std::cout << "\n  " << nr::dim("stream ended - 5 events") << '\n';
}

void simulated_hack(const std::string& host) {
  std::cout << "  [*] staging chain against " << host << '\n';
  nr::sleep_ms(nr::fast() ? 0 : 220);
  std::cout << "  [*] recon ....... ok\n";
  nr::sleep_ms(nr::fast() ? 0 : 220);
  std::cout << "  [*] payload ..... ok\n";
  nr::sleep_ms(nr::fast() ? 0 : 220);
  std::cout << "  [*] shell ....... ok\n\n";
  std::cout << "  uid=0(root) gid=0(root) groups=0(root)\n";
  std::cout << "  " << nr::dim("session key " + nr::hexs(24)) << '\n';
  std::cout << "  " << nr::ok("[+] shell established") << '\n';
}

bool run_tool(const std::string& name, const std::vector<std::string>& args) {
  if (name == "pe" || name == "hash" || name == "strings" || name == "hex" ||
      name == "find") {
    show(name, args);
    return true;
  }
  if (name == "init") {
    simulated_init();
    return true;
  }
  if (name == "scan") {
    simulated_scan(argument(args, "10.0.0.14"));
    return true;
  }
  if (name == "trace") {
    simulated_trace(argument(args, "10.0.0.14"));
    return true;
  }
  if (name == "crack") {
    simulated_crack(argument(args, "creds.enc"));
    return true;
  }
  if (name == "dump") {
    simulated_dump(argument(args, "eth0"));
    return true;
  }
  if (name == "watch") {
    simulated_watch(argument(args, "fw"));
    return true;
  }
  if (name == "hack") {
    simulated_hack(argument(args, "10.0.0.14"));
    return true;
  }
  return false;
}

}

int main() {
  nr::init();

  if (nr::deps::first_run()) {
    nr::deps::check(true);
    nr::deps::mark_installed();
  }

  nr::setup_window();
  if (nr::tty()) nr::startup_dialog_sequence();

  nr::clear_screen();
  nr::print_mini_ascii();
  std::cout << "  " << nr::dim("legacy terminal - local only") << "\n\n";
  std::cout << "  type " << nr::strong("help") << " for commands, "
            << nr::strong("tools") << " for the toolkit." << "\n\n";

  std::string line;
  for (;;) {
    std::cout << nr::ok("nullroute:~#") << " " << std::flush;
    if (!std::getline(std::cin, line)) break;
    std::string command = trim(line);
    if (command.empty()) continue;

    std::vector<std::string> parts = split(command);
    std::string name = parts[0];
    std::vector<std::string> args(parts.begin() + 1, parts.end());

    if (name == "exit" || name == "quit") break;
    if (name == "clear" || name == "cls") {
      nr::clear_screen();
      nr::print_mini_ascii();
      std::cout << '\n';
      continue;
    }
    if (name == "help") {
      print_help();
      continue;
    }
    if (name == "tools" || name == "menu") {
      print_tools();
      continue;
    }

    bool numeric = !name.empty();
    for (char c : name) {
      if (c < '0' || c > '9') numeric = false;
    }
    if (numeric) {
      int index = std::atoi(name.c_str());
      if (index >= 1 && index <= kToolCount) {
        run_tool(kTools[index - 1].command, args);
        continue;
      }
    }

    if (run_tool(name, args)) continue;

    std::cout << "  " << nr::err("unknown command") << '\n';
  }

  std::cout << "  " << nr::dim("session closed") << '\n';
  return 0;
}
