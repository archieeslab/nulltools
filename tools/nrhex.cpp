#include "nr/nr.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage() {
  nr::out("nrhex - hex dump a file and locate bytes");
  nr::out();
  nr::out("  usage  nrhex <file> [--offset <n>] [--length <n>] [--find <text>]");
  nr::out("                       [--no-color]");
  nr::out();
  nr::out("  offset accepts decimal or 0x hex. reads only the requested window.");
}

std::uint64_t parse_number(const std::string& value) {
  if (value.size() > 2 && value[0] == '0' &&
      (value[1] == 'x' || value[1] == 'X')) {
    return std::stoull(value.substr(2), nullptr, 16);
  }
  return std::stoull(value);
}

std::string hex_offset(std::uint64_t value) {
  static const char* digits = "0123456789abcdef";
  std::string out(8, '0');
  for (int i = 7; i >= 0 && value > 0; --i) {
    out[static_cast<std::size_t>(i)] = digits[value & 0x0f];
    value >>= 4;
  }
  return out;
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string path;
  std::string needle;
  std::uint64_t offset = 0;
  std::size_t length = 256;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--offset" && i + 1 < argc) {
      offset = parse_number(argv[++i]);
    } else if (arg == "--length" && i + 1 < argc) {
      length = static_cast<std::size_t>(parse_number(argv[++i]));
    } else if (arg == "--find" && i + 1 < argc) {
      needle = argv[++i];
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    } else if (path.empty()) {
      path = arg;
    }
  }

  if (path.empty()) {
    usage();
    return 2;
  }
  if (length == 0) length = 256;
  if (length > 65536) length = 65536;

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "nrhex: " << path << ": cannot open\n";
    return 2;
  }

  input.seekg(0, std::ios::end);
  std::uint64_t file_size = static_cast<std::uint64_t>(input.tellg());
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

  std::vector<std::uint8_t> window(length);
  input.read(reinterpret_cast<char*>(window.data()),
             static_cast<std::streamsize>(length));
  window.resize(static_cast<std::size_t>(input.gcount()));

  std::vector<std::uint64_t> hits;
  if (!needle.empty()) {
    input.clear();
    input.seekg(0, std::ios::beg);
    std::string data;
    char chunk[65536];
    while (input) {
      input.read(chunk, sizeof(chunk));
      data.append(chunk, static_cast<std::size_t>(input.gcount()));
    }
    std::size_t position = data.find(needle);
    while (position != std::string::npos && hits.size() < 32) {
      hits.push_back(static_cast<std::uint64_t>(position));
      position = data.find(needle, position + 1);
    }
  }

  nr::head("nrhex " + path);
  nr::out();
  nr::kv("size", nr::human_size(static_cast<long long>(file_size)));
  nr::kv("window", nr::human_size(static_cast<long long>(window.size())) +
                       " at 0x" + hex_offset(offset));
  if (!needle.empty()) {
    nr::kv("find", "\"" + needle + "\" (" + std::to_string(hits.size()) +
                       " hits)");
  }
  nr::out();

  for (std::size_t line = 0; line < window.size(); line += 16) {
    std::uint64_t base = offset + static_cast<std::uint64_t>(line);
    std::string bytes;
    std::string text;

    for (std::size_t i = 0; i < 16; ++i) {
      if (i == 8) {
        bytes += ' ';
      }
      if (line + i < window.size()) {
        unsigned char value = window[line + i];
        std::uint64_t position = base + i;
        bool marked =
            !needle.empty() &&
            std::any_of(hits.begin(), hits.end(), [&](std::uint64_t hit) {
              return position >= hit && position < hit + needle.size();
            });

        static const char* digits = "0123456789abcdef";
        std::string pair;
        pair += digits[value >> 4];
        pair += digits[value & 0x0f];
        bytes += marked ? nr::ok(pair) : pair;
        bytes += ' ';

        char glyph = (value >= 32 && value < 127) ? static_cast<char>(value) : '.';
        text += marked ? nr::ok(std::string(1, glyph)) : std::string(1, glyph);
      } else {
        bytes += "   ";
        text += ' ';
      }
    }

    std::cout << "  " << nr::dim(hex_offset(base)) << "  " << bytes << " "
              << nr::dim(text) << '\n';
  }

  nr::out();
  nr::summary(std::to_string(window.size()) + " bytes - offset 0x" +
              hex_offset(offset) + " - " + path);
  return 0;
}
