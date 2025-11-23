#pragma once

#include "types.h"

#include <string>
#include <string_view>
#include <vector>

namespace compressup {

class ICompressor {
public:
    virtual ~ICompressor() = default;

    virtual std::string name() const = 0;
    virtual std::vector<Byte> compress(std::string_view input) = 0;
    virtual std::string decompress(const std::vector<Byte>& input) = 0;
};

} // namespace compressup
