#include "nr/nr.hpp"

#include <cctype>
#include <iostream>
#include <string>

namespace {

void usage() {
  nr::out("nrdump - packet capture (simulated)");
  nr::out();
  nr::out("  usage  nrdump [iface] [--count <n>] [--fast] [--no-color] [--seed <n>] [--dialog]");
  nr::out();
  nr::out("  no interface is opened. frames are generated locally.");
}

std::string stamp(double seconds) {
  int whole = static_cast<int>(seconds);
  int ms = static_cast<int>((seconds - whole) * 1000.0);
  int minutes = whole / 60;
  int secs = whole % 60;
  std::string out;
  out += minutes < 10 ? "0" : "";
  out += std::to_string(minutes);
  out += ":";
  out += secs < 10 ? "0" : "";
  out += std::to_string(secs);
  out += ".";
  out += ms < 100 ? "0" : "";
  out += ms < 10 ? "0" : "";
  out += std::to_string(ms);
  return out;
}

void hexdump(const std::string& bytes) {
  static const char* digits = "0123456789abcdef";
  for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
    std::string hex;
    std::string ascii;
    for (std::size_t i = 0; i < 16; ++i) {
      if (i == 8) hex += " ";
      if (offset + i < bytes.size()) {
        unsigned char c = static_cast<unsigned char>(bytes[offset + i]);
        hex += digits[c >> 4];
        hex += digits[c & 0x0f];
        hex += " ";
        ascii += std::isprint(c) ? static_cast<char>(c) : '.';
      } else {
        hex += "   ";
        ascii += ' ';
      }
    }
    std::string number = std::to_string(offset);
    while (number.size() < 4) number = "0" + number;
    std::cout << "  " << nr::dim(number) << "  " << hex << " " << nr::dim(ascii)
              << '\n';
  }
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string iface;
  int count = 10;
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
    } else if (arg == "--count" && i + 1 < argc) {
      try {
        count = std::stoi(argv[++i]);
      } catch (...) {
        std::cerr << "invalid count\n";
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
    } else if (iface.empty()) {
      iface = arg;
    }
  }
  if (iface.empty()) iface = "eth0";
  if (count < 1) count = 1;
  if (count > 500) count = 500;

  nr::head("nrdump " + iface);
  nr::out();

  nr::Table table({"time", "proto", "src", "dst", "bytes"});

  double clock = 0.0;
  long long total_bytes = 0;

  for (int i = 0; i < count; ++i) {
    clock += 0.02 + nr::rnd(90) / 1000.0;
    std::string proto = nr::rnd(4) == 0 ? "udp" : "tcp";
    std::string src = "10.0.0.2:" + std::to_string(49152 + nr::rnd(16000));
    std::string dst = std::string("10.0.0.14:") + (nr::rnd(2) ? "443" : "22");
    int bytes = 60 + nr::rnd(1200);
    total_bytes += bytes;
    table.add({stamp(clock), proto, src, dst, std::to_string(bytes)});
  }

  table.print_header();
  for (std::size_t i = 0; i < table.size(); ++i) {
    nr::sleep_ms(55);
    table.print_row(i);
  }

  std::string payload;
  payload.resize(64);
  for (std::size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<char>(nr::rnd(256));
  }

  nr::out();
  hexdump(payload);
  nr::out();
  std::string outcome = nr::num(count) + " packets - " +
                        nr::fixed(total_bytes / 1024.0, 1) + " KB - " + iface;
  nr::summary(outcome);
  if (show_dialog && nr::tty()) {
    nr::dialog(outcome + "\n\nNo interface was opened.", "nrdump",
               nr::Icon::info);
  }
  return 0;
}
