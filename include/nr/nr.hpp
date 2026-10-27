#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nr {

void init();
void enable_color(bool on);
bool color();
bool tty();
void set_fast(bool on);
bool fast();
void sleep_ms(int ms);
int term_width();
double now_ms();

std::string dim(const std::string& s);
std::string strong(const std::string& s);
std::string ok(const std::string& s);
std::string warn(const std::string& s);
std::string err(const std::string& s);

enum class Icon { info, warning, error };

bool dialog(const std::string& text, const std::string& title = "nullroute",
            Icon icon = Icon::info, const std::string& button = "OK");
void startup_dialog_sequence();
void clear_screen();
void setup_window();
void print_mini_ascii();

void out(const std::string& line = "");
void kv(const std::string& key, const std::string& value, int key_width = 10);
void head(const std::string& left, const std::string& right = "");
void summary(const std::string& text);

class Table {
 public:
  explicit Table(std::vector<std::string> headers);
  void add(std::vector<std::string> row);
  void print_header() const;
  void print_row(std::size_t index) const;
  void print_all() const;
  std::size_t size() const { return rows_.size(); }

 private:
  std::vector<std::string> headers_;
  std::vector<std::vector<std::string>> rows_;
  std::vector<std::size_t> widths_;
  void measure();
};

std::uint64_t rng_next();
void seed_rng(std::uint64_t seed);
int rnd(int bound);
std::string hexs(int length);
std::string fake_ip(const std::string& key);
std::string fake_host(const std::string& key);

std::string num(long long value);
std::string fixed(double value, int places);
std::string human_size(long long bytes);
std::string pad_right(const std::string& s, std::size_t width);
std::string pad_left(const std::string& s, std::size_t width);

double entropy(const std::uint8_t* data, std::size_t size);
double entropy_of(const std::uint64_t* counts, std::uint64_t total);
std::string magic_type(const std::uint8_t* data, std::size_t size);

}
