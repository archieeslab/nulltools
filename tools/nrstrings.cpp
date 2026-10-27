#include "nr/nr.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

void usage() {
  nr::out("nrstrings - extract printable strings from a file");
  nr::out();
  nr::out("  usage  nrstrings <file> [--min <n>] [--grep <term>] [--unique]");
  nr::out("                          [--max <n>] [--no-color]");
  nr::out();
  nr::out("  reads one file, prints runs of printable bytes.");
}

bool printable(unsigned char c) { return (c >= 32 && c < 127) || c == '\t'; }

std::string lower(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

std::string hex_offset(std::size_t value) {
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
  std::string filter;
  int minimum = 4;
  int maximum = 500;
  bool unique = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--min" && i + 1 < argc) {
      minimum = std::stoi(argv[++i]);
    } else if (arg == "--max" && i + 1 < argc) {
      maximum = std::stoi(argv[++i]);
    } else if (arg == "--grep" && i + 1 < argc) {
      filter = argv[++i];
    } else if (arg == "--unique") {
      unique = true;
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
  if (minimum < 2) minimum = 2;

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "nrstrings: " << path << ": cannot open\n";
    return 2;
  }

  std::vector<std::string> found;
  std::set<std::string> seen;
  std::string current;
  std::size_t offset = 0;
  std::size_t start = 0;
  std::size_t total = 0;

  char chunk[65536];
  while (input) {
    input.read(chunk, sizeof(chunk));
    std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      unsigned char c = static_cast<unsigned char>(chunk[i]);
      std::size_t position = offset + static_cast<std::size_t>(i);
      if (printable(c)) {
        if (current.empty()) start = position;
        current += static_cast<char>(c);
      } else if (!current.empty()) {
        if (static_cast<int>(current.size()) >= minimum) {
          bool keep = filter.empty() ||
                      lower(current).find(lower(filter)) != std::string::npos;
          if (keep && (!unique || seen.insert(current).second)) {
            found.push_back(hex_offset(start) + "  " + current);
            if (static_cast<int>(found.size()) >= maximum) break;
          }
        }
        current.clear();
      }
    }
    total += static_cast<std::size_t>(count);
    offset += static_cast<std::size_t>(count);
    if (static_cast<int>(found.size()) >= maximum) break;
  }

  if (static_cast<int>(current.size()) >= minimum &&
      static_cast<int>(found.size()) < maximum) {
    bool keep =
        filter.empty() || lower(current).find(lower(filter)) != std::string::npos;
    if (keep && (!unique || seen.insert(current).second)) {
      found.push_back(hex_offset(start) + "  " + current);
    }
  }

  nr::head("nrstrings " + path);
  nr::out();
  nr::kv("min length", std::to_string(minimum));
  if (!filter.empty()) nr::kv("grep", filter);
  nr::out();

  for (const std::string& line : found) {
    std::string offset_text = line.substr(0, 8);
    std::string text = line.substr(10);
    std::cout << "  " << nr::dim(offset_text) << "  " << text << '\n';
  }

  nr::out();
  nr::summary(nr::num(static_cast<long long>(found.size())) + " strings - " +
              nr::human_size(static_cast<long long>(total)) + " - " + path);
  return 0;
}
