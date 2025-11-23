#pragma once

#include "compressor.h"

namespace compressup {

class RleCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;
};

} // namespace compressup
