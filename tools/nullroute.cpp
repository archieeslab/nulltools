#include "nr/nr.hpp"
#include "nr/web.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  nr::init();

  bool launch = true;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--no-browser") {
      launch = false;
    } else if (arg == "-h" || arg == "--help" || arg == "help") {
      nr::out("nullroute - local console");
      nr::out();
      nr::out("  usage  nullroute                 serve and open the console");
      nr::out("         nullroute --no-browser    serve without opening a browser");
      nr::out("         nullroute --help          this text");
      nr::out();
      nr::out("  the page is served from 127.0.0.1 on the first free port");
      nr::out("  between 7800 and 7899. chrome is preferred, then edge.");
      nr::out();
      nr::out("  terminal build: nullroute-legacy.exe");
      return 0;
    } else {
      std::cerr << "nullroute: unknown option: " << arg << '\n';
      return 2;
    }
  }

  return nr::web::serve(launch);
}
