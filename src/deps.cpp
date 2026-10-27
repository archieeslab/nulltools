#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nr/deps.hpp"

#include "nr/nr.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>
#endif

namespace {

std::string state_dir() {
#ifdef _WIN32
  const char* local = std::getenv("LOCALAPPDATA");
  if (local != nullptr && local[0] != '\0') {
    return std::string(local) + "\\nullroute";
  }
  return std::string(".");
#else
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::string(home) + "/.local/share/nullroute";
  }
  return std::string(".");
#endif
}

std::string state_file() {
#ifdef _WIN32
  return state_dir() + "\\environment.txt";
#else
  return state_dir() + "/environment.txt";
#endif
}

#ifdef _WIN32

std::wstring widen(const std::string& text) {
  if (text.empty()) return std::wstring();
  int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &out[0], size);
  return out;
}

std::wstring read_app_path(const wchar_t* name) {
  std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
  key += name;

  for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    HKEY handle = nullptr;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_READ, &handle) != ERROR_SUCCESS) {
      continue;
    }
    wchar_t buffer[1024];
    DWORD bytes = sizeof(buffer);
    DWORD type = 0;
    LONG status = RegQueryValueExW(handle, nullptr, nullptr, &type,
                                   reinterpret_cast<LPBYTE>(buffer), &bytes);
    RegCloseKey(handle);
    if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) &&
        bytes > sizeof(wchar_t)) {
      return std::wstring(buffer, bytes / sizeof(wchar_t) - 1);
    }
  }
  return std::wstring();
}

std::wstring env_path(const wchar_t* name) {
  wchar_t buffer[1024];
  DWORD size = GetEnvironmentVariableW(name, buffer, 1024);
  if (size == 0 || size >= 1024) return std::wstring();
  return std::wstring(buffer, size);
}

std::wstring first_existing(const std::vector<std::wstring>& roots,
                            const std::wstring& tail) {
  for (const std::wstring& root : roots) {
    if (root.empty()) continue;
    std::wstring candidate = root + tail;
    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
      return candidate;
    }
  }
  return std::wstring();
}

#endif

}

namespace nr {
namespace deps {

std::string chrome_path() {
#ifdef _WIN32
  std::wstring registered = read_app_path(L"chrome.exe");
  if (!registered.empty()) {
    int size = WideCharToMultiByte(CP_UTF8, 0, registered.data(),
                                   static_cast<int>(registered.size()), nullptr,
                                   0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, registered.data(),
                        static_cast<int>(registered.size()), &out[0], size,
                        nullptr, nullptr);
    return out;
  }

  std::vector<std::wstring> roots = {env_path(L"ProgramFiles"),
                                     env_path(L"ProgramFiles(x86)"),
                                     env_path(L"LOCALAPPDATA")};
  std::wstring found =
      first_existing(roots, L"\\Google\\Chrome\\Application\\chrome.exe");
  if (!found.empty()) {
    int size = WideCharToMultiByte(CP_UTF8, 0, found.data(),
                                   static_cast<int>(found.size()), nullptr, 0,
                                   nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, found.data(), static_cast<int>(found.size()),
                        &out[0], size, nullptr, nullptr);
    return out;
  }
#endif
  return std::string();
}

std::string edge_path() {
#ifdef _WIN32
  std::wstring registered = read_app_path(L"msedge.exe");
  std::wstring found = registered;
  if (found.empty()) {
    std::vector<std::wstring> roots = {env_path(L"ProgramFiles(x86)"),
                                       env_path(L"ProgramFiles")};
    found = first_existing(roots, L"\\Microsoft\\Edge\\Application\\msedge.exe");
  }
  if (!found.empty()) {
    int size = WideCharToMultiByte(CP_UTF8, 0, found.data(),
                                   static_cast<int>(found.size()), nullptr, 0,
                                   nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, found.data(), static_cast<int>(found.size()),
                        &out[0], size, nullptr, nullptr);
    return out;
  }
#else
  const char* candidates[] = {"/usr/bin/google-chrome", "/usr/bin/chromium",
                              "/usr/bin/microsoft-edge", "/usr/bin/chromium-browser"};
  for (const char* path : candidates) {
    if (std::ifstream(path).good()) return path;
  }
#endif
  return std::string();
}

bool first_run() {
  std::ifstream marker(state_file());
  return !marker.good();
}

void mark_installed() {
#ifdef _WIN32
  CreateDirectoryW(widen(state_dir()).c_str(), nullptr);
#else
  std::string command = "mkdir -p \"" + state_dir() + "\"";
  std::system(command.c_str());
#endif
  std::ofstream out(state_file());
  if (!out) return;
  std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char stamp[64] = {0};
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
  out << "version 1.0.0\n";
  out << "checked " << stamp << "\n";
  out << "runtime static\n";
}

bool check(bool interactive) {
  bool ok = true;
  std::string problems;

#ifdef _WIN32
  using rtl_get_version = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  rtl_get_version version_fn = nullptr;
  if (ntdll != nullptr) {
    version_fn = reinterpret_cast<rtl_get_version>(
        GetProcAddress(ntdll, "RtlGetVersion"));
  }

  OSVERSIONINFOW info{};
  info.dwOSVersionInfoSize = sizeof(info);
  if (version_fn != nullptr) version_fn(&info);

  bool modern = info.dwMajorVersion >= 10 &&
                (info.dwMajorVersion > 10 || info.dwBuildNumber >= 17763);
  if (!modern) {
    ok = false;
    problems += "windows 10 build 17763 or newer is required.\n";
  }

  struct ModuleCheck {
    const wchar_t* wide;
    const char* narrow;
  };
  const ModuleCheck modules[] = {{L"ws2_32.dll", "ws2_32.dll"},
                                 {L"user32.dll", "user32.dll"},
                                 {L"gdi32.dll", "gdi32.dll"},
                                 {L"shell32.dll", "shell32.dll"},
                                 {L"advapi32.dll", "advapi32.dll"}};
  for (const ModuleCheck& module : modules) {
    HMODULE handle = LoadLibraryW(module.wide);
    if (handle == nullptr) {
      ok = false;
      problems += std::string("missing system module: ") + module.narrow + "\n";
    } else {
      FreeLibrary(handle);
    }
  }
#else
  (void)interactive;
#endif

  if (!ok && interactive) {
    dialog(problems + "\nnullroute links its runtime statically, so this is a "
                      "system problem rather than a missing installer.",
           "nullroute setup", Icon::error);
  }

  return ok;
}

bool offer_browser_download() {
  bool accepted = dialog(
      "No Chrome or Edge was found on this machine.\n\n"
      "The console needs a browser. Open the official Chrome download page?",
      "nullroute setup", Icon::warning, "DOWNLOAD");
  if (accepted) open_url("https://www.google.com/chrome/");
  return accepted;
}

void open_url(const std::string& url) {
#ifdef _WIN32
  ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
#else
  std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
  std::system(command.c_str());
#endif
}

}
}
