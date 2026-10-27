#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nr/nr.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#include <windowsx.h>
#else
#include <unistd.h>
#endif

namespace nr {
namespace {

std::mt19937_64 g_rng{std::random_device{}()};
bool g_color = true;
bool g_fast = false;
bool g_tty = false;

const char* kDim = "38;5;244";
const char* kStrong = "1";
const char* kOk = "38;5;114";
const char* kWarn = "38;5;179";
const char* kErr = "38;5;174";

std::string sgr(const char* code, const std::string& s) {
  if (!g_color) return s;
  return std::string("\x1b[") + code + "m" + s + "\x1b[0m";
}

std::size_t display_width(const std::string& s) { return s.size(); }

#ifdef _WIN32
std::wstring widen(const std::string& s) {
  if (s.empty()) return std::wstring();
  int size = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                 static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                      &out[0], size);
  return out;
}
#endif

}

void init() {
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode)) {
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
  SetConsoleOutputCP(CP_UTF8);
  g_tty = _isatty(_fileno(stdout)) != 0;
  g_color = g_tty;
#else
  g_tty = isatty(fileno(stdout)) != 0;
  g_color = g_tty;
#endif
  std::ios::sync_with_stdio(false);
}

void enable_color(bool on) { g_color = on; }
bool color() { return g_color; }
bool tty() { return g_tty; }

void set_fast(bool on) { g_fast = on; }
bool fast() { return g_fast; }

void sleep_ms(int ms) {
  if (g_fast || ms <= 0) return;
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int term_width() {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    int width = info.srWindow.Right - info.srWindow.Left + 1;
    if (width > 20) return width;
  }
#endif
  if (const char* env = std::getenv("COLUMNS")) {
    int width = std::atoi(env);
    if (width > 20) return width;
  }
  return 100;
}

double now_ms() {
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  return std::chrono::duration<double, std::milli>(clock::now() - start).count();
}

std::string dim(const std::string& s) { return sgr(kDim, s); }
std::string strong(const std::string& s) { return sgr(kStrong, s); }
std::string ok(const std::string& s) { return sgr(kOk, s); }
std::string warn(const std::string& s) { return sgr(kWarn, s); }
std::string err(const std::string& s) { return sgr(kErr, s); }

void out(const std::string& line) { std::cout << line << '\n'; }

void kv(const std::string& key, const std::string& value, int key_width) {
  std::size_t width = static_cast<std::size_t>(key_width > 0 ? key_width : 0);
  width = std::max(width, key.size() + 2);
  std::cout << "  " << dim(pad_right(key, width)) << value << '\n';
}

void head(const std::string& left, const std::string& right) {
  std::cout << strong(left);
  if (!right.empty()) {
    int gap = term_width() - static_cast<int>(left.size()) -
              static_cast<int>(right.size());
    if (gap < 1) gap = 1;
    std::cout << std::string(static_cast<std::size_t>(gap), ' ') << dim(right);
  }
  std::cout << '\n';
}

void summary(const std::string& text) { std::cout << "  " << dim(text) << '\n'; }

#ifdef _WIN32

namespace {

HHOOK g_cbt_hook = nullptr;
std::wstring g_cbt_button;

LRESULT CALLBACK cbt_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HCBT_ACTIVATE) {
    HWND dlg = reinterpret_cast<HWND>(wparam);
    if (!g_cbt_button.empty()) {
      SetDlgItemTextW(dlg, IDOK, g_cbt_button.c_str());
    }
    if (g_cbt_hook) {
      UnhookWindowsHookEx(g_cbt_hook);
      g_cbt_hook = nullptr;
    }
  }
  return CallNextHookEx(g_cbt_hook, code, wparam, lparam);
}

}

#endif

bool dialog(const std::string& text, const std::string& title, Icon icon,
            const std::string& button) {
#ifdef _WIN32
  (void)icon;
  UINT flags = MB_OK | MB_SETFOREGROUND | MB_TOPMOST | MB_USERICON;
  const std::wstring wide_text = widen(text);
  const std::wstring wide_title = widen(title);
  if (!button.empty() && button != "OK") {
    g_cbt_button = widen(button);
    g_cbt_hook = SetWindowsHookExW(WH_CBT, cbt_hook_proc, nullptr,
                                   GetCurrentThreadId());
  }
  int res = 0;
  MSGBOXPARAMSW params{};
  params.cbSize = sizeof(params);
  params.hwndOwner = nullptr;
  params.hInstance = GetModuleHandleW(nullptr);
  params.lpszText = wide_text.c_str();
  params.lpszCaption = wide_title.c_str();
  params.dwStyle = flags;
  params.lpszIcon = MAKEINTRESOURCEW(1);
  params.dwContextHelpId = 0;
  params.lpfnMsgBoxCallback = nullptr;
  params.dwLanguageId = 0;
  res = MessageBoxIndirectW(&params);
  if (g_cbt_hook) {
    UnhookWindowsHookEx(g_cbt_hook);
    g_cbt_hook = nullptr;
  }
  return res == IDOK;
#else
  (void)icon;
  std::cout << "  [" << title << "] " << text << " (" << button << ")\n";
  return true;
#endif
}

void startup_dialog_sequence() {
  dialog("Loading...\n\nStarting the console service and reading settings.",
         "nullroute setup", Icon::info, "NEXT");
  dialog("Unpacking...\n\nPreparing the bundled analysis tools.",
         "nullroute setup", Icon::info, "NEXT");
  dialog("Nextup...\n\nLinking the command interface to the local engine.",
         "nullroute setup", Icon::info, "NEXT");
  dialog("Opening...\n\nListening on 127.0.0.1, waiting for the console.",
         "nullroute setup", Icon::info, "NEXT");
}

void clear_screen() {
  if (!g_tty) return;
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
      DWORD count = 0;
      DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
      COORD home = {0, 0};
      FillConsoleOutputCharacterA(hOut, ' ', cells, home, &count);
      FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, home, &count);
      SetConsoleCursorPosition(hOut, home);
      return;
    }
  }
#endif
  std::cout << "\033[2J\033[H" << std::flush;
}

void setup_window() {
#ifdef _WIN32
  SetConsoleTitleW(L"nullroute // reverse engineering console [v1.0.0]");
  HWND hwnd = GetConsoleWindow();
  if (hwnd) {
    HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                   IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (icon) {
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
      SMALL_RECT minRect = {0, 0, 1, 1};
      SetConsoleWindowInfo(hOut, TRUE, &minRect);
      COORD coord = {86, 28};
      SetConsoleScreenBufferSize(hOut, coord);
      SMALL_RECT rect = {0, 0, 85, 27};
      SetConsoleWindowInfo(hOut, TRUE, &rect);
    }
    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
      int w = rc.right - rc.left;
      int h = rc.bottom - rc.top;
      int sx = GetSystemMetrics(SM_CXSCREEN);
      int sy = GetSystemMetrics(SM_CYSCREEN);
      SetWindowPos(hwnd, nullptr, (sx - w) / 2, (sy - h) / 2, 0, 0,
                   SWP_NOZORDER | SWP_NOSIZE);
    }
  }
#endif
}

void print_mini_ascii() {
  const std::string c_dark = g_color ? "\033[38;5;242m" : "";
  const std::string c_rst = g_color ? "\033[0m" : "";
  std::cout << c_dark
            << R"(  _   _  _   _  _      _      ____    ___   _   _  _____  _____
  | \ | || | | || |    | |    |  _ \  / _ \ | | | ||_   _|| ____|
  |  \| || | | || |    | |    | |_) || | | || | | |  | |  |  _|
  | |\  || |_| || |___ | |___ |  _ < | |_| || |_| |  | |  | |___
  |_| \_| \___/ |_____||_____||_| \_\ \___/  \___/   |_|  |_____|
)"
            << c_rst << '\n';
}

Table::Table(std::vector<std::string> headers) : headers_(std::move(headers)) {
  widths_.assign(headers_.size(), 0);
  measure();
}

void Table::measure() {
  for (std::size_t i = 0; i < headers_.size(); ++i) {
    widths_[i] = display_width(headers_[i]);
  }
  for (const auto& row : rows_) {
    for (std::size_t i = 0; i < row.size() && i < widths_.size(); ++i) {
      widths_[i] = std::max(widths_[i], display_width(row[i]));
    }
  }
}

void Table::add(std::vector<std::string> row) {
  rows_.push_back(std::move(row));
  measure();
}

void Table::print_header() const {
  std::string line = "  ";
  for (std::size_t i = 0; i < headers_.size(); ++i) {
    if (i) line += "  ";
    line += headers_[i];
    if (i + 1 < headers_.size()) {
      line += std::string(widths_[i] - display_width(headers_[i]) + 2, ' ');
    }
  }
  std::cout << dim(line) << '\n';
}

void Table::print_row(std::size_t index) const {
  if (index >= rows_.size()) return;
  const auto& row = rows_[index];
  std::string line = "  ";
  for (std::size_t i = 0; i < row.size(); ++i) {
    if (i) line += "  ";
    line += row[i];
    if (i + 1 < row.size() && i < widths_.size()) {
      line += std::string(widths_[i] - display_width(row[i]) + 2, ' ');
    }
  }
  std::cout << line << '\n';
}

void Table::print_all() const {
  print_header();
  for (std::size_t i = 0; i < rows_.size(); ++i) print_row(i);
}

std::uint64_t rng_next() { return g_rng(); }

void seed_rng(std::uint64_t seed) { g_rng.seed(seed); }

int rnd(int bound) {
  if (bound <= 0) return 0;
  return static_cast<int>(g_rng() % static_cast<std::uint64_t>(bound));
}

std::string hexs(int length) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  s.reserve(static_cast<std::size_t>(length));
  for (int i = 0; i < length; ++i) s += digits[rnd(16)];
  return s;
}

namespace {

std::uint32_t fnv1a(const std::string& s) {
  std::uint32_t hash = 2166136261u;
  for (unsigned char c : s) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}

}

std::string fake_ip(const std::string& key) {
  std::uint32_t hash = fnv1a(key);
  int b = 1 + static_cast<int>((hash >> 8) % 250);
  int c = 1 + static_cast<int>((hash >> 16) % 250);
  int d = 1 + static_cast<int>((hash >> 24) % 250);
  return "10." + std::to_string(b) + "." + std::to_string(c) + "." +
         std::to_string(d);
}

std::string fake_host(const std::string& key) {
  static const char* words[] = {"edge", "core", "node", "relay",
                                "gw",   "svc",  "brd",  "gw"};
  static const char* zones[] = {"internal", "lan", "corp", "local"};
  std::uint32_t hash = fnv1a(key);
  std::string name = words[hash % 8];
  int number = 10 + static_cast<int>((hash >> 8) % 90);
  return name + "-" + (number < 10 ? "0" : "") + std::to_string(number) + "." +
         zones[(hash >> 16) % 4];
}

std::string num(long long value) {
  bool negative = value < 0;
  std::string s = std::to_string(negative ? -value : value);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
    s.insert(static_cast<std::size_t>(i), ",");
  }
  return negative ? "-" + s : s;
}

std::string fixed(double value, int places) {
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(places);
  ss << value;
  return ss.str();
}

std::string human_size(long long bytes) {
  static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << " "
     << units[unit];
  return ss.str();
}

std::string pad_right(const std::string& s, std::size_t width) {
  if (s.size() >= width) return s;
  return s + std::string(width - s.size(), ' ');
}

std::string pad_left(const std::string& s, std::size_t width) {
  if (s.size() >= width) return s;
  return std::string(width - s.size(), ' ') + s;
}

double entropy_of(const std::uint64_t* counts, std::uint64_t total) {
  if (total == 0) return 0.0;
  double result = 0.0;
  for (int i = 0; i < 256; ++i) {
    if (counts[i] == 0) continue;
    double p = static_cast<double>(counts[i]) / static_cast<double>(total);
    result -= p * std::log2(p);
  }
  return result;
}

double entropy(const std::uint8_t* data, std::size_t size) {
  std::uint64_t counts[256] = {0};
  for (std::size_t i = 0; i < size; ++i) ++counts[data[i]];
  return entropy_of(counts, static_cast<std::uint64_t>(size));
}

std::string magic_type(const std::uint8_t* data, std::size_t size) {
  if (size == 0) return "empty";
  if (size >= 8 && std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) {
    return "png image";
  }
  if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) {
    return "jpeg image";
  }
  if (size >= 4 && std::memcmp(data, "GIF8", 4) == 0) return "gif image";
  if (size >= 5 && std::memcmp(data, "%PDF-", 5) == 0) return "pdf document";
  if (size >= 4 && data[0] == 'P' && data[1] == 'K' &&
      (data[2] == 3 || data[2] == 5) && (data[3] == 4 || data[3] == 6)) {
    return "zip archive";
  }
  if (size >= 4 && std::memcmp(data, "\x7f" "ELF", 4) == 0) {
    return "elf binary";
  }
  if (size >= 2 && data[0] == 'M' && data[1] == 'Z') return "pe executable";
  if (size >= 16 && std::memcmp(data, "SQLite format 3", 16) == 0) {
    return "sqlite database";
  }
  if (size >= 2 && data[0] == 0x1f && data[1] == 0x8b) return "gzip archive";
  if (size >= 12 && std::memcmp(data, "RIFF", 4) == 0 &&
      std::memcmp(data + 8, "WEBP", 4) == 0) {
    return "webp image";
  }
  if (size >= 4 && std::memcmp(data, "OggS", 4) == 0) return "ogg stream";
  if (size >= 3 && std::memcmp(data, "ID3", 3) == 0) return "mp3 audio";

  bool text = true;
  for (std::size_t i = 0; i < size && i < 512; ++i) {
    unsigned char c = data[i];
    if (c == 0 || (c < 9) || (c > 13 && c < 32)) {
      text = false;
      break;
    }
  }
  return text ? "text" : "binary";
}

}
