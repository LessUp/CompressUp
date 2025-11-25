#pragma once

#include "compressor.h"

namespace compressup {

// BWT (Burrows-Wheeler Transform) 结合 MTF (Move-to-Front) 编码
// BWT将输入重新排列使相同字符聚集，MTF利用局部性原理编码
class BwtCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;
    
    // 块大小限制
    static constexpr std::size_t kMaxBlockSize = 100000;

private:
    // BWT变换
    std::pair<std::string, std::size_t> bwt_transform(std::string_view input) const;
    
    // BWT逆变换
    std::string bwt_inverse(std::string_view input, std::size_t primary_index) const;
    
    // MTF编码
    std::vector<Byte> mtf_encode(std::string_view input) const;
    
    // MTF解码
    std::string mtf_decode(const std::vector<Byte>& input) const;
};

} // namespace compressup
