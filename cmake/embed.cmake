file(SIZE "${NR_ASSET_IN}" NR_ASSET_SIZE)
file(READ "${NR_ASSET_IN}" NR_ASSET_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," NR_ASSET_BYTES "${NR_ASSET_HEX}")

file(WRITE "${NR_ASSET_OUT}"
"#include <cstddef>

namespace nr {
namespace assets {

extern const unsigned char ${NR_ASSET_NAME}[] = {${NR_ASSET_BYTES}};
extern const unsigned long ${NR_ASSET_NAME}_size = ${NR_ASSET_SIZE}UL;

}
}
")
