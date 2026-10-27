#include "nr/nr.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Match {
  std::string path;
  std::string size;
  std::string modified;
  std::string snippet;
  double entropy;
};

void usage() {
  nr::out("nrfind - search files by name or content");
  nr::out();
  nr::out("  usage  nrfind <query> [--root <dir>] [--content] [--ext <ext>]");
  nr::out("                        [--depth <n>] [--max <n>] [--hidden]");
  nr::out("                        [--entropy] [--no-color] [--fast]");
  nr::out();
  nr::out("  pattern accepts * and ? wildcards. reads only the given root.");
}

std::string lower(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

bool contains_ci(const std::string& text, const std::string& needle) {
  if (needle.empty()) return true;
  return lower(text).find(lower(needle)) != std::string::npos;
}

bool glob_match(const std::string& pattern, const std::string& text) {
  std::size_t p = 0;
  std::size_t t = 0;
  std::size_t star = std::string::npos;
  std::size_t mark = 0;

  while (t < text.size()) {
    if (p < pattern.size() &&
        (pattern[p] == '?' ||
         std::tolower(static_cast<unsigned char>(pattern[p])) ==
             std::tolower(static_cast<unsigned char>(text[t])))) {
      ++p;
      ++t;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      mark = t;
    } else if (star != std::string::npos) {
      p = star + 1;
      t = ++mark;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') ++p;
  return p == pattern.size();
}

bool name_matches(const std::string& pattern, const std::string& name) {
  bool wildcard =
      pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
  return wildcard ? glob_match(pattern, name) : contains_ci(name, pattern);
}

std::string stamp(fs::file_time_type value) {
  auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      value - fs::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  std::time_t seconds = std::chrono::system_clock::to_time_t(system);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &seconds);
#else
  localtime_r(&seconds, &local);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
  return buffer;
}

std::vector<std::uint8_t> read_bytes(const fs::path& path, std::size_t limit) {
  std::ifstream input(path, std::ios::binary);
  std::vector<std::uint8_t> data;
  if (!input) return data;
  data.resize(limit);
  input.read(reinterpret_cast<char*>(data.data()),
             static_cast<std::streamsize>(limit));
  data.resize(static_cast<std::size_t>(input.gcount()));
  return data;
}

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

std::string find_line(const std::string& text, const std::string& needle) {
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = trim(text.substr(start, end - start));
    if (contains_ci(line, needle)) {
      if (line.size() > 78) line = line.substr(0, 77) + "~";
      return line;
    }
    start = end + 1;
  }
  return std::string();
}

std::string normalize_ext(const std::string& ext) {
  if (ext.empty()) return ext;
  std::string value = lower(ext);
  if (value[0] != '.') value = "." + value;
  return value;
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string query;
  std::string root = ".";
  std::string ext;
  bool content = false;
  bool hidden = false;
  bool with_entropy = false;
  int depth = 12;
  int max_matches = 40;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--root" && i + 1 < argc) {
      root = argv[++i];
    } else if (arg == "--ext" && i + 1 < argc) {
      ext = normalize_ext(argv[++i]);
    } else if (arg == "--depth" && i + 1 < argc) {
      depth = std::stoi(argv[++i]);
    } else if (arg == "--max" && i + 1 < argc) {
      max_matches = std::stoi(argv[++i]);
    } else if (arg == "--content") {
      content = true;
    } else if (arg == "--hidden") {
      hidden = true;
    } else if (arg == "--entropy") {
      with_entropy = true;
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    } else if (query.empty()) {
      query = arg;
    }
  }

  if (query.empty()) {
    usage();
    return 2;
  }

  std::error_code error;
  if (!fs::is_directory(root, error)) {
    std::cerr << "nrfind: " << root << ": not a directory\n";
    return 2;
  }

  const auto started = nr::now_ms();
  long long scanned = 0;
  std::vector<Match> matches;

  fs::recursive_directory_iterator iterator(
      root, fs::directory_options::skip_permission_denied, error);
  const fs::recursive_directory_iterator finish;

  for (; iterator != finish; iterator.increment(error)) {
    if (error) {
      error.clear();
      continue;
    }

    const fs::directory_entry& entry = *iterator;
    const std::string name = entry.path().filename().string();

    if (!hidden && !name.empty() && name[0] == '.') {
      if (entry.is_directory(error)) iterator.disable_recursion_pending();
      continue;
    }
    if (entry.is_directory(error)) {
      if (iterator.depth() >= depth) iterator.disable_recursion_pending();
      continue;
    }
    if (!entry.is_regular_file(error)) continue;

    ++scanned;

    if (!ext.empty() && lower(entry.path().extension().string()) != ext) {
      continue;
    }

    std::vector<std::uint8_t> bytes;
    std::string snippet;

    if (content) {
      bytes = read_bytes(entry.path(), 8 * 1024 * 1024);
      if (bytes.empty()) continue;
      std::string payload(bytes.begin(), bytes.end());
      if (payload.find('\0') != std::string::npos) continue;
      snippet = find_line(payload, query);
      if (snippet.empty()) continue;
    } else {
      if (!name_matches(query, name)) continue;
      if (with_entropy) {
        bytes = read_bytes(entry.path(), 4 * 1024 * 1024);
      }
    }

    Match match;
    match.path = fs::relative(entry.path(), root).generic_string();
    match.size = nr::human_size(static_cast<long long>(entry.file_size(error)));
    match.modified = stamp(entry.last_write_time(error));
    match.snippet = content ? snippet : std::string();
    match.entropy =
        with_entropy ? nr::entropy(bytes.data(), bytes.size()) : 0.0;

    matches.push_back(match);
    if (static_cast<int>(matches.size()) >= max_matches) break;
  }

  std::sort(matches.begin(), matches.end(),
            [](const Match& a, const Match& b) { return a.path < b.path; });

  nr::head("nrfind " + query);
  nr::out();
  nr::kv("root", root);
  nr::kv("mode", content ? "content" : "name");
  if (!ext.empty()) nr::kv("ext", ext);
  nr::out();

  if (!matches.empty()) {
    std::vector<std::string> headers = {"path", "size", "modified"};
    if (with_entropy) headers.push_back("entropy");
    nr::Table table(headers);
    for (const Match& match : matches) {
      std::vector<std::string> row = {match.path, match.size, match.modified};
      if (with_entropy) row.push_back(nr::fixed(match.entropy, 2));
      table.add(row);
    }
    table.print_header();
    for (std::size_t i = 0; i < table.size(); ++i) {
      nr::sleep_ms(12);
      table.print_row(i);
      if (!matches[i].snippet.empty()) {
        std::cout << "    " << nr::dim(matches[i].snippet) << '\n';
      }
    }
  }

  double elapsed = (nr::now_ms() - started) / 1000.0;
  nr::out();
  nr::summary(nr::num(static_cast<long long>(matches.size())) + " matches - " +
              nr::num(scanned) + " files scanned - " + nr::fixed(elapsed, 2) +
              "s");
  return 0;
}
