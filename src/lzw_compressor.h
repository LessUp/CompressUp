#pragma once

#include "compressor.h"

#include <unordered_map>

namespace compressup {

class LzwCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;

private:
    // 初始字典大小 (0-255 的单字符)
    static constexpr std::size_t kInitialDictSize = 256;
    // 最大字典大小 (12位编码 = 4096)
    static constexpr std::size_t kMaxDictSize = 4096;
    // 编码位数
    static constexpr int kCodeBits = 12;
    
    // 将编码写入比特流
    void write_code(std::vector<Byte>& output, std::uint16_t code, 
                   int& bit_buffer, int& bits_in_buffer) const;
    
    // 从比特流读取编码
    std::uint16_t read_code(const Byte*& data, const Byte* end,
                           int& bit_buffer, int& bits_in_buffer) const;
};

} // namespace compressup
