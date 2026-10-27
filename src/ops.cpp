#include "nr/ops.hpp"

#include "nr/hash.hpp"
#include "nr/nr.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace nr {
namespace ops {
namespace {

std::string hex_width(std::uint64_t value, int digits) {
  static const char* table = "0123456789abcdef";
  std::string out(static_cast<std::size_t>(digits), '0');
  for (int i = digits - 1; i >= 0 && value > 0; --i) {
    out[static_cast<std::size_t>(i)] = table[value & 0x0f];
    value >>= 4;
  }
  return out;
}

std::string lower(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

bool is_flag(const std::string& arg) {
  return !arg.empty() && arg[0] == '-' && arg.size() > 1;
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

std::uint64_t parse_u64(const std::string& text) {
  if (text.size() > 2 && text[0] == '0' &&
      (text[1] == 'x' || text[1] == 'X')) {
    return std::stoull(text.substr(2), nullptr, 16);
  }
  return std::stoull(text);
}

std::vector<std::uint8_t> read_window(const fs::path& path, std::uint64_t offset,
                                      std::size_t length) {
  std::vector<std::uint8_t> data;
  std::ifstream input(path, std::ios::binary);
  if (!input) return data;
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  data.resize(length);
  input.read(reinterpret_cast<char*>(data.data()),
             static_cast<std::streamsize>(length));
  data.resize(static_cast<std::size_t>(input.gcount()));
  return data;
}

std::string stamp(fs::file_time_type value) {
  auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      value - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
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

struct Section {
  std::string name;
  std::uint32_t virt_addr;
  std::uint32_t raw_addr;
  std::uint32_t raw_size;
  std::uint32_t flags;
  double entropy;
};

std::uint32_t to_offset(std::uint32_t rva, const std::vector<Section>& sections) {
  for (const Section& s : sections) {
    if (rva >= s.virt_addr && rva < s.virt_addr + s.raw_size) {
      return s.raw_addr + (rva - s.virt_addr);
    }
  }
  return 0;
}

std::string permissions(std::uint32_t flags) {
  std::string out;
  out += (flags & 0x40000000) ? "R" : "-";
  out += (flags & 0x80000000) ? "W" : "-";
  out += (flags & 0x20000000) ? "X" : "-";
  return out;
}

const char* machine_name(std::uint16_t machine) {
  switch (machine) {
    case 0x8664:
      return "AMD64";
    case 0x014c:
      return "i386";
    case 0xaa64:
      return "ARM64";
    default:
      return "unknown";
  }
}

std::string header_line(const std::string& label, const std::string& value) {
  return "  " + pad_right(label, 13) + value + "\n";
}

}

std::string pe(const std::vector<std::string>& args) {
  std::string path;
  for (const std::string& arg : args) {
    if (!is_flag(arg) && path.empty()) path = arg;
  }
  if (path.empty()) return "error: usage: pe <file>\n";

  std::ifstream input(path, std::ios::binary);
  if (!input) return "error: " + path + ": cannot open\n";

  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 64) return "error: " + path + ": too small for a dos header\n";
  if (data[0] != 'M' || data[1] != 'Z') {
    return "error: " + path + ": no mz signature, not a pe file\n";
  }

  std::uint32_t pe_offset = 0;
  std::memcpy(&pe_offset, &data[0x3c], 4);
  if (pe_offset + 24 > data.size()) {
    return "error: " + path + ": e_lfanew points outside the file\n";
  }
  if (std::memcmp(&data[pe_offset], "PE\0\0", 4) != 0) {
    return "error: " + path + ": no pe signature\n";
  }

  const std::uint8_t* coff = &data[pe_offset + 4];
  std::uint16_t machine = 0;
  std::uint16_t sections_count = 0;
  std::uint16_t opt_size = 0;
  std::memcpy(&machine, &coff[0], 2);
  std::memcpy(&sections_count, &coff[2], 2);
  std::memcpy(&opt_size, &coff[16], 2);

  const std::uint8_t* opt = coff + 20;
  std::uint16_t magic = 0;
  std::uint32_t entry = 0;
  std::uint64_t image_base = 0;
  std::uint16_t subsystem = 0;
  std::uint16_t dll_flags = 0;
  std::uint32_t import_rva = 0;

  if (opt_size >= 2) {
    std::memcpy(&magic, &opt[0], 2);
    bool wide = magic == 0x020b;
    std::memcpy(&entry, &opt[16], 4);
    if (wide) {
      std::memcpy(&image_base, &opt[24], 8);
      std::memcpy(&subsystem, &opt[68], 2);
      std::memcpy(&dll_flags, &opt[70], 2);
      if (opt_size >= 124) std::memcpy(&import_rva, &opt[120], 4);
    } else {
      std::uint32_t base32 = 0;
      std::memcpy(&base32, &opt[28], 4);
      image_base = base32;
      std::memcpy(&subsystem, &opt[68], 2);
      std::memcpy(&dll_flags, &opt[70], 2);
      if (opt_size >= 108) std::memcpy(&import_rva, &opt[104], 4);
    }
  }

  const std::uint8_t* table = opt + opt_size;
  std::vector<Section> sections;

  for (std::uint16_t i = 0; i < sections_count; ++i) {
    const std::uint8_t* raw = table + i * 40;
    if (raw + 40 > data.data() + data.size()) break;
    Section s;
    char name[9] = {0};
    std::memcpy(name, raw, 8);
    s.name = name;
    std::memcpy(&s.raw_size, &raw[16], 4);
    s.virt_addr = 0;
    std::memcpy(&s.virt_addr, &raw[12], 4);
    std::memcpy(&s.raw_addr, &raw[20], 4);
    std::memcpy(&s.flags, &raw[36], 4);
    s.entropy = 0.0;
    if (s.raw_size > 0 && s.raw_addr + s.raw_size <= data.size()) {
      s.entropy = entropy(&data[s.raw_addr], s.raw_size);
    }
    sections.push_back(s);
  }

  std::vector<std::string> modules;
  if (import_rva != 0) {
    std::uint32_t at = to_offset(import_rva, sections);
    while (at != 0 && at + 20 <= data.size()) {
      std::uint32_t name_rva = 0;
      std::memcpy(&name_rva, &data[at + 12], 4);
      if (name_rva == 0) break;
      std::uint32_t name_at = to_offset(name_rva, sections);
      if (name_at != 0 && name_at < data.size()) {
        modules.push_back(reinterpret_cast<const char*>(&data[name_at]));
      }
      at += 20;
    }
  }

  std::string out;
  out += "  " + path + "\n\n";
  out += header_line("size", human_size(static_cast<long long>(data.size())));
  out += header_line("machine", machine_name(machine));
  out += header_line("format", magic == 0x020b ? "PE32+ (64-bit)" : "PE32 (32-bit)");
  out += header_line("subsystem", subsystem == 3 ? "console" : "windows gui");
  out += header_line("entry", "0x" + hex_width(entry, 8));
  out += header_line("image base", "0x" + hex_width(image_base, magic == 0x020b ? 16 : 8));

  std::string flags;
  if (dll_flags & 0x0040) flags += "aslr ";
  if (dll_flags & 0x0100) flags += "dep ";
  if (dll_flags & 0x4000) flags += "cfg ";
  if (dll_flags & 0x0020) flags += "highentropy ";
  out += header_line("mitigations", flags.empty() ? "none" : trim(flags));
  out += "\n";

  out += "  name        virt-addr   raw-size   flags   entropy\n";
  for (const Section& s : sections) {
    out += "  " + pad_right(s.name, 12) + pad_right("0x" + hex_width(s.virt_addr, 8), 12) +
           pad_right(human_size(static_cast<long long>(s.raw_size)), 11) +
           pad_right(permissions(s.flags), 8) + fixed(s.entropy, 2) + "\n";
  }

  if (!modules.empty()) {
    out += "\n  imports\n";
    for (const std::string& module : modules) {
      out += "    -> " + module + "\n";
    }
  }

  out += "\n[+] " + std::to_string(sections.size()) + " sections, " +
         std::to_string(modules.size()) + " modules\n";
  return out;
}

std::string hash(const std::vector<std::string>& args) {
  std::string path;
  bool with_md5 = false;
  for (const std::string& arg : args) {
    if (arg == "--md5") {
      with_md5 = true;
    } else if (!is_flag(arg) && path.empty()) {
      path = arg;
    }
  }
  if (path.empty()) return "error: usage: hash <file> [--md5]\n";

  std::ifstream input(path, std::ios::binary);
  if (!input) return "error: " + path + ": cannot open\n";

  Sha256 sha;
  Md5 md5;
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

  std::string out;
  out += "  " + path + "\n\n";
  out += header_line("size", human_size(static_cast<long long>(total)));
  out += header_line("type", magic_type(head, head_size));
  out += header_line("entropy", fixed(entropy_of(counts, total), 2));
  out += header_line("sha256", sha.finish());
  if (with_md5) out += header_line("md5", md5.finish());
  return out;
}

std::string strings(const std::vector<std::string>& args) {
  std::string path;
  std::string filter;
  int minimum = 4;
  int maximum = 400;
  bool unique = false;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--min" && i + 1 < args.size()) {
      minimum = std::stoi(args[++i]);
    } else if (arg == "--grep" && i + 1 < args.size()) {
      filter = args[++i];
    } else if (arg == "--max" && i + 1 < args.size()) {
      maximum = std::stoi(args[++i]);
    } else if (arg == "--unique") {
      unique = true;
    } else if (!is_flag(arg) && path.empty()) {
      path = arg;
    }
  }

  if (path.empty()) return "error: usage: strings <file> [--min n] [--grep term]\n";
  if (minimum < 2) minimum = 2;

  std::ifstream input(path, std::ios::binary);
  if (!input) return "error: " + path + ": cannot open\n";

  std::vector<std::string> found;
  std::vector<std::string> seen;
  std::string current;
  std::size_t start = 0;
  std::size_t position = 0;
  std::uint64_t total = 0;

  char chunk[65536];
  while (input) {
    input.read(chunk, sizeof(chunk));
    std::streamsize got = input.gcount();
    for (std::streamsize i = 0; i < got; ++i) {
      unsigned char c = static_cast<unsigned char>(chunk[i]);
      bool printable = (c >= 32 && c < 127) || c == '\t';
      if (printable) {
        if (current.empty()) start = position;
        current += static_cast<char>(c);
      } else if (!current.empty()) {
        if (static_cast<int>(current.size()) >= minimum) {
          bool keep = filter.empty() ||
                      lower(current).find(lower(filter)) != std::string::npos;
          bool fresh = !unique ||
                       std::find(seen.begin(), seen.end(), current) == seen.end();
          if (keep && fresh) {
            seen.push_back(current);
            found.push_back(hex_width(start, 8) + "  " + current);
            if (static_cast<int>(found.size()) >= maximum) break;
          }
        }
        current.clear();
      }
      ++position;
    }
    total += static_cast<std::uint64_t>(got);
    if (static_cast<int>(found.size()) >= maximum) break;
  }

  if (static_cast<int>(current.size()) >= minimum &&
      static_cast<int>(found.size()) < maximum) {
    bool keep = filter.empty() ||
                lower(current).find(lower(filter)) != std::string::npos;
    if (keep) found.push_back(hex_width(start, 8) + "  " + current);
  }

  std::string out;
  out += "  " + path + "  (" + human_size(static_cast<long long>(total)) + ")\n\n";
  for (const std::string& line : found) {
    out += "  " + line.substr(0, 8) + "  " + line.substr(10) + "\n";
  }
  out += "\n[+] " + std::to_string(found.size()) + " strings\n";
  return out;
}

std::string hex(const std::vector<std::string>& args) {
  std::string path;
  std::string needle;
  std::uint64_t offset = 0;
  std::size_t length = 256;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--offset" && i + 1 < args.size()) {
      offset = parse_u64(args[++i]);
    } else if (arg == "--length" && i + 1 < args.size()) {
      length = static_cast<std::size_t>(parse_u64(args[++i]));
    } else if (arg == "--find" && i + 1 < args.size()) {
      needle = args[++i];
    } else if (!is_flag(arg) && path.empty()) {
      path = arg;
    }
  }

  if (path.empty()) return "error: usage: hex <file> [--offset n] [--length n]\n";
  if (length == 0) length = 256;
  if (length > 65536) length = 65536;

  std::ifstream input(path, std::ios::binary);
  if (!input) return "error: " + path + ": cannot open\n";

  input.seekg(0, std::ios::end);
  std::uint64_t size = static_cast<std::uint64_t>(input.tellg());

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
    std::size_t at = data.find(needle);
    while (at != std::string::npos && hits.size() < 16) {
      hits.push_back(static_cast<std::uint64_t>(at));
      at = data.find(needle, at + 1);
    }
  }

  std::vector<std::uint8_t> window = read_window(path, offset, length);

  std::string out;
  out += "  " + path + "\n\n";
  out += header_line("size", human_size(static_cast<long long>(size)));
  out += header_line("window", std::to_string(window.size()) + " B at 0x" +
                                  hex_width(offset, 8));
  if (!needle.empty()) {
    out += header_line("find", "\"" + needle + "\" (" + std::to_string(hits.size()) +
                                   " hits)");
  }
  out += "\n";

  static const char* digits = "0123456789abcdef";
  for (std::size_t line = 0; line < window.size(); line += 16) {
    std::uint64_t base = offset + line;
    std::string bytes;
    std::string text;

    for (std::size_t i = 0; i < 16; ++i) {
      if (i == 8) bytes += ' ';
      if (line + i < window.size()) {
        unsigned char value = window[line + i];
        bytes += digits[value >> 4];
        bytes += digits[value & 0x0f];
        bytes += ' ';
        text += (value >= 32 && value < 127) ? static_cast<char>(value) : '.';
      } else {
        bytes += "   ";
        text += ' ';
      }
    }

    out += "  " + hex_width(base, 8) + "  " + bytes + " " + text + "\n";
  }

  out += "\n[+] " + std::to_string(window.size()) + " bytes shown\n";
  return out;
}

std::string find(const std::vector<std::string>& args) {
  std::string query;
  std::string root = ".";
  std::string ext;
  bool content = false;
  int limit = 40;
  int depth = 12;

  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--root" && i + 1 < args.size()) {
      root = args[++i];
    } else if (arg == "--ext" && i + 1 < args.size()) {
      ext = lower(args[++i]);
      if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    } else if (arg == "--max" && i + 1 < args.size()) {
      limit = std::stoi(args[++i]);
    } else if (arg == "--depth" && i + 1 < args.size()) {
      depth = std::stoi(args[++i]);
    } else if (arg == "--content") {
      content = true;
    } else if (!is_flag(arg) && query.empty()) {
      query = arg;
    }
  }

  if (query.empty()) return "error: usage: find <query> [--root dir] [--content]\n";

  std::error_code error;
  if (!fs::is_directory(root, error)) return "error: " + root + ": not a directory\n";

  std::vector<std::string> rows;
  int scanned = 0;

  fs::recursive_directory_iterator it(
      root, fs::directory_options::skip_permission_denied, error);
  const fs::recursive_directory_iterator end;

  for (; it != end; it.increment(error)) {
    if (error) {
      error.clear();
      continue;
    }
    const fs::directory_entry& entry = *it;

    if (entry.is_directory(error)) {
      if (it.depth() >= depth) it.disable_recursion_pending();
      continue;
    }
    if (!entry.is_regular_file(error)) continue;
    if (!ext.empty() && lower(entry.path().extension().string()) != ext) continue;

    ++scanned;
    std::string name = entry.path().filename().string();

    if (content) {
      std::ifstream input(entry.path(), std::ios::binary);
      if (!input) continue;
      std::string payload((std::istreambuf_iterator<char>(input)),
                          std::istreambuf_iterator<char>());
      if (payload.find('\0') != std::string::npos) continue;
      if (lower(payload).find(lower(query)) == std::string::npos) continue;
    } else {
      if (lower(name).find(lower(query)) == std::string::npos) continue;
    }

    rows.push_back(fs::relative(entry.path(), root).generic_string() + "  " +
                   human_size(static_cast<long long>(entry.file_size(error))) + "  " +
                   stamp(entry.last_write_time(error)));
    if (static_cast<int>(rows.size()) >= limit) break;
  }

  std::string out;
  out += "  root " + root + "   query " + query + "\n\n";
  for (const std::string& row : rows) out += "  " + row + "\n";
  out += "\n[+] " + std::to_string(rows.size()) + " matches, " +
         std::to_string(scanned) + " files scanned\n";
  return out;
}

std::string run(const std::string& command, const std::vector<std::string>& args) {
  if (command == "pe") return pe(args);
  if (command == "hash") return hash(args);
  if (command == "strings") return strings(args);
  if (command == "hex") return hex(args);
  if (command == "find") return find(args);
  if (command == "ready") return "ok\n";
  return "error: unknown operation: " + command + "\n";
}

}
}
