#pragma once

#include <string>

namespace nr {
namespace deps {

std::string chrome_path();
std::string edge_path();
bool first_run();
void mark_installed();
bool check(bool interactive);
bool offer_browser_download();
void open_url(const std::string& url);

}
}
