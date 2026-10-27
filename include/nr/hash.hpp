#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nr {

class Sha256 {
 public:
  Sha256();
  void update(const void* data, std::size_t size);
  std::string finish();

 private:
  std::uint32_t state_[8];
  std::uint64_t total_;
  std::uint8_t buffer_[64];
  std::size_t used_;
  void block(const std::uint8_t* chunk);
};

class Md5 {
 public:
  Md5();
  void update(const void* data, std::size_t size);
  std::string finish();

 private:
  std::uint32_t state_[4];
  std::uint64_t total_;
  std::uint8_t buffer_[64];
  std::size_t used_;
  void block(const std::uint8_t* chunk);
};

}
