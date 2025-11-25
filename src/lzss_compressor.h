#pragma once

#include "compressor.h"

namespace compressup {

// LZSS: LZ77的变体，只有当匹配长度超过阈值时才使用引用
class LzssCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;

    // 参数配置
    static constexpr std::size_t kWindowSize = 4096;     // 滑动窗口大小
    static constexpr std::size_t kLookAheadSize = 18;    // 前向缓冲区大小
    static constexpr std::size_t kMinMatchLength = 3;    // 最小匹配长度阈值
    
private:
    // 查找最长匹配
    std::pair<std::size_t, std::size_t> find_longest_match(
        std::string_view input, std::size_t pos) const;
};

} // namespace compressup
