#pragma once

#include "block.h"

#include <functional>

namespace nenet {

using BlockReader = std::function<Block(int, int, int)>;

}
