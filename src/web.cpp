#include "nr/web.hpp"

#include "nr/assets.hpp"
#include "nr/deps.hpp"
#include "nr/nr.hpp"
#include "nr/ops.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using socket_type = SOCKET;
const socket_type kInvalidSocket = INVALID_SOCKET;

std::wstring widen(const std::string& text) {
  if (text.empty()) return std::wstring();
  int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &out[0], size);
  return out;
}

std::string narrow(const std::wstring& text) {
  if (text.empty()) return std::string();
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0,
                                 nullptr, nullptr);
  std::string out(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &out[0], size, nullptr, nullptr);
  return out;
}
#else
using socket_type = int;
const socket_type kInvalidSocket = -1;
#endif

void close_socket(socket_type socket) {
#ifdef _WIN32
  closesocket(socket);
#else
  ::close(socket);
#endif
}

bool would_block() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

std::string lowercase(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

std::string url_decode(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '+') {
      out += ' ';
    } else if (text[i] == '%' && i + 2 < text.size()) {
      int value = 0;
      std::string hex = text.substr(i + 1, 2);
      for (char c : hex) {
        value <<= 4;
        if (c >= '0' && c <= '9') {
          value += c - '0';
        } else if (c >= 'a' && c <= 'f') {
          value += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
          value += c - 'A' + 10;
        }
      }
      out += static_cast<char>(value);
      i += 2;
    } else {
      out += text[i];
    }
  }
  return out;
}

std::string random_token() {
  static const char* digits = "0123456789abcdef";
  std::string token;
  for (int i = 0; i < 20; ++i) {
    token += digits[nr::rng_next() & 0x0f];
  }
  return token;
}

std::string response(int code, const std::string& type, const std::string& body) {
  const char* reason = code == 200 ? "OK" : (code == 404 ? "Not Found" : "Error");
  std::string out = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n";
  out += "Content-Type: " + type + "\r\n";
  out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  out += "Cache-Control: no-store\r\n";
  out += "Connection: close\r\n\r\n";
  out += body;
  return out;
}

bool send_all(socket_type socket, const std::string& data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    int chunk = static_cast<int>(
        send(socket, data.data() + sent, static_cast<int>(data.size() - sent), 0));
    if (chunk <= 0) return false;
    sent += static_cast<std::size_t>(chunk);
  }
  return true;
}

std::string header_value(const std::string& request, const std::string& name) {
  std::string target = lowercase(name) + ":";
  std::size_t at = 0;
  while (at < request.size()) {
    std::size_t end = request.find("\r\n", at);
    if (end == std::string::npos) break;
    std::string line = request.substr(at, end - at);
    std::string probe = lowercase(line);
    if (probe.compare(0, target.size(), target) == 0) {
      std::size_t value = line.find(':');
      if (value != std::string::npos) {
        std::string trimmed = line.substr(value + 1);
        while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) {
          trimmed.erase(trimmed.begin());
        }
        while (!trimmed.empty() &&
               (trimmed.back() == ' ' || trimmed.back() == '\t')) {
          trimmed.pop_back();
        }
        return trimmed;
      }
    }
    at = end + 2;
  }
  return std::string();
}

#ifdef _WIN32
std::wstring browser_profile() {
  wchar_t buffer[1024];
  DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 1024);
  std::wstring base = size > 0 ? std::wstring(buffer, size) : std::wstring(L".");
  std::wstring root = base + L"\\nullroute";
  std::wstring dir = root + L"\\browser";
  CreateDirectoryW(root.c_str(), nullptr);
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

bool launch_app(const std::wstring& exe, const std::string& url) {
  if (exe.empty()) return false;
  std::wstring profile = browser_profile();
  std::wstring args = L"\"" + exe + L"\" --app=" + widen(url) +
                      L" --user-data-dir=\"" + profile + L"\"" +
                      L" --window-size=900,560 --no-first-run"
                      L" --no-default-browser-check --disable-features=Translate";
  std::vector<wchar_t> buffer(args.begin(), args.end());
  buffer.push_back(L'\0');

  STARTUPINFOW info{};
  info.cb = sizeof(info);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(exe.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &info, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

bool open_browser(const std::string& url) {
  std::string chrome = nr::deps::chrome_path();
  if (!chrome.empty() && launch_app(widen(chrome), url)) {
    std::cout << "  browser    chrome\n";
    return true;
  }
  std::string edge = nr::deps::edge_path();
  if (!edge.empty() && launch_app(widen(edge), url)) {
    std::cout << "  browser    edge\n";
    return true;
  }
  HINSTANCE status = ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr,
                                   nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<intptr_t>(status) > 32) {
    std::cout << "  browser    default\n";
    return true;
  }
  return false;
}
#else
bool open_browser(const std::string& url) {
  std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
  return std::system(command.c_str()) == 0;
}
#endif

bool g_stop = false;
std::string g_token;

std::string handle_request(const std::string& request, bool& quit) {
  std::size_t line_end = request.find("\r\n");
  if (line_end == std::string::npos) return response(400, "text/plain", "bad request\n");

  std::string line = request.substr(0, line_end);
  std::size_t first = line.find(' ');
  std::size_t second = line.find(' ', first + 1);
  if (first == std::string::npos || second == std::string::npos) {
    return response(400, "text/plain", "bad request\n");
  }

  std::string target = line.substr(first + 1, second - first - 1);
  std::string query;
  std::size_t mark = target.find('?');
  if (mark != std::string::npos) {
    query = target.substr(mark + 1);
    target = target.substr(0, mark);
  }

  if (target == "/" || target == "/index.html") {
    return response(200, "text/html; charset=utf-8",
                    std::string(reinterpret_cast<const char*>(nr::assets::index_html),
                                nr::assets::index_html_size));
  }

  if (target == "/logo.png") {
    return response(200, "image/png",
                    std::string(reinterpret_cast<const char*>(nr::assets::logo_png),
                                nr::assets::logo_png_size));
  }

  if (target != "/api/run") {
    return response(404, "text/plain", "not found\n");
  }

  std::string token = header_value(request, "X-NR-Token");
  if (token != g_token) {
    return response(403, "text/plain", "error: bad token\n");
  }

  std::string command;
  std::vector<std::string> args;
  std::size_t at = 0;
  while (at <= query.size()) {
    std::size_t end = query.find('&', at);
    if (end == std::string::npos) end = query.size();
    std::string pair = query.substr(at, end - at);
    std::size_t equals = pair.find('=');
    if (equals != std::string::npos) {
      std::string key = url_decode(pair.substr(0, equals));
      std::string value = url_decode(pair.substr(equals + 1));
      if (key == "cmd") {
        command = value;
      } else if (key == "arg") {
        args.push_back(value);
      }
    }
    if (end == query.size()) break;
    at = end + 1;
  }

  if (command.empty()) return response(400, "text/plain", "error: missing command\n");
  if (command == "quit") {
    quit = true;
    return response(200, "text/plain", "ok\n");
  }

  return response(200, "text/plain; charset=utf-8", nr::ops::run(command, args));
}

socket_type make_listener(int& port) {
  socket_type listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == kInvalidSocket) return kInvalidSocket;

#ifndef _WIN32
  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  for (int candidate = 7800; candidate < 7900; ++candidate) {
    address.sin_port = htons(static_cast<unsigned short>(candidate));
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
      port = candidate;
      if (listen(listener, 8) == 0) return listener;
    }
  }

  address.sin_port = 0;
  if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    close_socket(listener);
    return kInvalidSocket;
  }
  listen(listener, 8);

  sockaddr_in actual{};
#ifdef _WIN32
  int length = sizeof(actual);
#else
  socklen_t length = sizeof(actual);
#endif
  getsockname(listener, reinterpret_cast<sockaddr*>(&actual), &length);
  port = ntohs(actual.sin_port);
  return listener;
}

}

namespace nr {
namespace web {

int serve(bool launch_browser) {
  const bool fresh = deps::first_run();
  if (fresh) {
    deps::check(true);
    deps::mark_installed();
  }

#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::cerr << "nullroute: winsock init failed\n";
    return 2;
  }
#endif

  int port = 0;
  socket_type listener = make_listener(port);
  if (listener == kInvalidSocket) {
    std::cerr << "nullroute: cannot bind a local port\n";
#ifdef _WIN32
    WSACleanup();
#endif
    return 2;
  }

  g_token = random_token();
  std::string url = "http://127.0.0.1:" + std::to_string(port) + "/?t=" + g_token;

  out();
  out("  " + strong("nullroute") + " " + dim("local console"));
  out();
  kv("address", "http://127.0.0.1:" + std::to_string(port));
  kv("session", g_token);
  out();
  out("  keep this window open, closing it stops the server.");
  out();

  if (!launch_browser) {
    out("  " + dim("browser launch disabled"));
  } else if (!open_browser(url)) {
    out("  " + warn("no chrome or edge found, open the address above manually"));
    if (fresh) deps::offer_browser_download();
  }

  out("  " + dim("ctrl+c to stop"));
  out();
  std::cout.flush();

  while (!g_stop) {
    sockaddr_in remote{};
#ifdef _WIN32
    int length = sizeof(remote);
#else
    socklen_t length = sizeof(remote);
#endif
    socket_type client =
        accept(listener, reinterpret_cast<sockaddr*>(&remote), &length);
    if (client == kInvalidSocket) {
      if (would_block()) continue;
      break;
    }

    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == std::string::npos) {
      int got = recv(client, buffer, sizeof(buffer), 0);
      if (got <= 0) break;
      request.append(buffer, static_cast<std::size_t>(got));
      if (request.size() > 65536) break;
    }

    bool quit = false;
    std::string body = handle_request(request, quit);
    send_all(client, body);
    close_socket(client);
    if (quit) g_stop = true;
  }

  close_socket(listener);
#ifdef _WIN32
  WSACleanup();
#endif
  out("  " + dim("server stopped"));
  return 0;
}

}
}
