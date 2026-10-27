#pragma once

#include <string>
#include <vector>

namespace nr {
namespace ops {

std::string pe(const std::vector<std::string>& args);
std::string hash(const std::vector<std::string>& args);
std::string strings(const std::vector<std::string>& args);
std::string hex(const std::vector<std::string>& args);
std::string find(const std::vector<std::string>& args);
std::string run(const std::string& command, const std::vector<std::string>& args);

}
}
