#include "nr/nr.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct SectionInfo {
  std::string name;
  std::uint32_t virt_addr;
  std::uint32_t virt_size;
  std::uint32_t raw_addr;
  std::uint32_t raw_size;
  std::uint32_t characteristics;
  double entropy;
};

void usage() {
  nr::out("nrpe - inspect portable executable headers, sections & imports");
  nr::out();
  nr::out("  usage  nrpe <file> [--sections] [--imports] [--no-color]");
  nr::out();
  nr::out("  parses DOS, COFF, Optional headers, section tables and IAT.");
}

std::string hex_str(std::uint64_t val, int width = 8) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  for (int i = (width - 1) * 4; i >= 0; i -= 4) {
    out += digits[(val >> i) & 0x0f];
  }
  return "0x" + out;
}

std::uint32_t rva_to_offset(std::uint32_t rva, const std::vector<SectionInfo>& sections) {
  for (const auto& s : sections) {
    if (rva >= s.virt_addr && rva < s.virt_addr + s.virt_size) {
      return s.raw_addr + (rva - s.virt_addr);
    }
  }
  return 0;
}

std::string perm_str(std::uint32_t ch) {
  std::string p;
  p += (ch & 0x40000000) ? "R" : "-";
  p += (ch & 0x80000000) ? "W" : "-";
  p += (ch & 0x20000000) ? "X" : "-";
  return p;
}

std::string machine_name(std::uint16_t m) {
  if (m == 0x8664) return "AMD64 (x86-64)";
  if (m == 0x014c) return "i386 (x86 32-bit)";
  if (m == 0xaa64) return "ARM64";
  return "unknown (" + hex_str(m, 4) + ")";
}

std::string subsystem_name(std::uint16_t s) {
  if (s == 2) return "Windows GUI";
  if (s == 3) return "Windows CUI (Console)";
  return "subsystem " + std::to_string(s);
}

}

int main(int argc, char** argv) {
  nr::init();
  std::string path;
  bool show_imports = true;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--no-color") {
      nr::enable_color(false);
    } else if (arg == "--fast") {
      nr::set_fast(true);
    } else if (arg == "--no-imports") {
      show_imports = false;
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

  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::cerr << "nrpe: " << path << ": cannot open\n";
    return 2;
  }

  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 64) {
    std::cerr << "nrpe: " << path << ": file too small for DOS header\n";
    return 2;
  }

  if (data[0] != 'M' || data[1] != 'Z') {
    std::cerr << "nrpe: " << path << ": invalid DOS signature (missing MZ)\n";
    return 2;
  }

  std::uint32_t pe_offset = *reinterpret_cast<const std::uint32_t*>(&data[0x3c]);
  if (pe_offset + 24 > data.size()) {
    std::cerr << "nrpe: " << path << ": invalid e_lfanew offset\n";
    return 2;
  }

  if (data[pe_offset] != 'P' || data[pe_offset + 1] != 'E' ||
      data[pe_offset + 2] != 0 || data[pe_offset + 3] != 0) {
    std::cerr << "nrpe: " << path << ": invalid PE signature\n";
    return 2;
  }

  const std::uint8_t* coff = &data[pe_offset + 4];
  std::uint16_t machine = *reinterpret_cast<const std::uint16_t*>(&coff[0]);
  std::uint16_t num_sections = *reinterpret_cast<const std::uint16_t*>(&coff[2]);
  std::uint16_t opt_size = *reinterpret_cast<const std::uint16_t*>(&coff[16]);

  const std::uint8_t* opt = coff + 20;
  bool is_64 = false;
  std::uint16_t opt_magic = 0;
  std::uint32_t entry_point = 0;
  std::uint64_t image_base = 0;
  std::uint16_t subsystem = 0;
  std::uint16_t dll_char = 0;
  std::uint32_t import_rva = 0;

  if (opt_size >= 2) {
    opt_magic = *reinterpret_cast<const std::uint16_t*>(&opt[0]);
    is_64 = (opt_magic == 0x020b);
    entry_point = *reinterpret_cast<const std::uint32_t*>(&opt[16]);
    if (is_64) {
      image_base = *reinterpret_cast<const std::uint64_t*>(&opt[24]);
      subsystem = *reinterpret_cast<const std::uint16_t*>(&opt[68]);
      dll_char = *reinterpret_cast<const std::uint16_t*>(&opt[70]);
      if (opt_size >= 120) {
        import_rva = *reinterpret_cast<const std::uint32_t*>(&opt[120]);
      }
    } else {
      image_base = *reinterpret_cast<const std::uint32_t*>(&opt[28]);
      subsystem = *reinterpret_cast<const std::uint16_t*>(&opt[68]);
      dll_char = *reinterpret_cast<const std::uint16_t*>(&opt[70]);
      if (opt_size >= 104) {
        import_rva = *reinterpret_cast<const std::uint32_t*>(&opt[104]);
      }
    }
  }

  const std::uint8_t* sec_header = opt + opt_size;
  std::vector<SectionInfo> sections;

  for (std::uint16_t i = 0; i < num_sections; ++i) {
    const std::uint8_t* sh = sec_header + i * 40;
    if (sh + 40 > data.data() + data.size()) break;

    char name_buf[9] = {0};
    std::memcpy(name_buf, sh, 8);

    SectionInfo s;
    s.name = name_buf;
    s.virt_size = *reinterpret_cast<const std::uint32_t*>(&sh[8]);
    s.virt_addr = *reinterpret_cast<const std::uint32_t*>(&sh[12]);
    s.raw_size = *reinterpret_cast<const std::uint32_t*>(&sh[16]);
    s.raw_addr = *reinterpret_cast<const std::uint32_t*>(&sh[20]);
    s.characteristics = *reinterpret_cast<const std::uint32_t*>(&sh[36]);
    s.entropy = 0.0;

    if (s.raw_addr + s.raw_size <= data.size() && s.raw_size > 0) {
      s.entropy = nr::entropy(&data[s.raw_addr], s.raw_size);
    }
    sections.push_back(s);
  }

  std::vector<std::string> imported_dlls;
  if (show_imports && import_rva != 0) {
    std::uint32_t desc_offset = rva_to_offset(import_rva, sections);
    while (desc_offset != 0 && desc_offset + 20 <= data.size()) {
      std::uint32_t name_rva = *reinterpret_cast<const std::uint32_t*>(&data[desc_offset + 12]);
      if (name_rva == 0) break;
      std::uint32_t name_offset = rva_to_offset(name_rva, sections);
      if (name_offset != 0 && name_offset < data.size()) {
        const char* s = reinterpret_cast<const char*>(&data[name_offset]);
        imported_dlls.emplace_back(s);
      }
      desc_offset += 20;
    }
  }

  nr::head("nrpe " + path);
  nr::out();
  nr::kv("file size", nr::human_size(static_cast<long long>(data.size())));
  nr::kv("target", machine_name(machine));
  nr::kv("format", is_64 ? "PE32+ (64-bit)" : "PE32 (32-bit)");
  nr::kv("subsystem", subsystem_name(subsystem));
  nr::kv("entry point", hex_str(entry_point, 8));
  nr::kv("image base", hex_str(image_base, is_64 ? 16 : 8));
  nr::kv("sections", std::to_string(num_sections));

  std::string sec_flags;
  if (dll_char & 0x0040) sec_flags += "ASLR (DynamicBase) ";
  if (dll_char & 0x0100) sec_flags += "DEP (NX) ";
  if (dll_char & 0x4000) sec_flags += "CFG (Guard) ";
  if (dll_char & 0x0020) sec_flags += "HighEntropyVA ";
  if (sec_flags.empty()) sec_flags = "none";
  nr::kv("security", sec_flags);
  nr::out();

  nr::out("  " + nr::strong("sections"));
  nr::Table st({"name", "virt-addr", "raw-size", "flags", "entropy"});
  for (const auto& s : sections) {
    st.add({s.name, hex_str(s.virt_addr, 8), nr::human_size(s.raw_size),
            perm_str(s.characteristics), nr::fixed(s.entropy, 2)});
  }
  st.print_all();
  nr::out();

  if (!imported_dlls.empty()) {
    nr::out("  " + nr::strong("imports"));
    for (const auto& dll : imported_dlls) {
      nr::out("    " + nr::dim("->") + " " + dll);
    }
    nr::out();
  }

  nr::summary("analyzed " + path + " - " + std::to_string(sections.size()) +
              " sections, " + std::to_string(imported_dlls.size()) + " imported modules");
  return 0;
}
