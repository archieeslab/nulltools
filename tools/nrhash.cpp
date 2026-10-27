#include "nr/hash.hpp"
#include "nr/nr.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage() {
  nr::out("nrhash - hash, size, type and entropy for files");
  nr::out();
  nr::out("  usage  nrhash <file...> [--md5] [--no-color]");
  nr::out();
  nr::out("  reads files locally and prints sha256 by default.");
}

}

int main(int argc, char** argv) {
  nr::init();
  std::vector<std::string> files;
  bool with_md5 = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--md5") {
      with_md5 = true;
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    } else {
      files.push_back(arg);
    }
  }

  if (files.empty()) {
    usage();
    return 2;
  }

  int failures = 0;
  bool first = true;

  for (const std::string& file : files) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
      std::cerr << "nrhash: " << file << ": cannot open\n";
      ++failures;
      continue;
    }

    nr::Sha256 sha;
    nr::Md5 md5;
    std::uint64_t counts[256] = {0};
    std::uint64_t total = 0;
    std::uint8_t head[512];
    std::size_t head_size = 0;
    char chunk[65536];

    for (;;) {
      input.read(chunk, sizeof(chunk));
      std::streamsize got = input.gcount();
      if (got <= 0) break;

      sha.update(chunk, static_cast<std::size_t>(got));
      md5.update(chunk, static_cast<std::size_t>(got));

      const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(chunk);
      for (std::streamsize i = 0; i < got; ++i) ++counts[bytes[i]];

      if (head_size < sizeof(head)) {
        std::size_t take = std::min<std::size_t>(
            sizeof(head) - head_size, static_cast<std::size_t>(got));
        std::copy(bytes, bytes + take, head + head_size);
        head_size += take;
      }

      total += static_cast<std::uint64_t>(got);
    }

    if (!first) nr::out();
    first = false;

    nr::head("nrhash " + file);
    nr::out();
    nr::kv("size", nr::human_size(static_cast<long long>(total)));
    nr::kv("type", nr::magic_type(head, head_size));
    nr::kv("entropy", nr::fixed(nr::entropy_of(counts, total), 2));
    nr::kv("sha256", sha.finish());
    if (with_md5) nr::kv("md5", md5.finish());
  }

  return failures == 0 ? 0 : 2;
}
