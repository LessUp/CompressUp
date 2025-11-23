#pragma once

#include "compressor.h"

namespace compressup {

class Lz77Compressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;

    static constexpr std::size_t kWindowSize = 1024;
    static constexpr std::size_t kMaxMatchLength = 32;
};

} // namespace compressup
