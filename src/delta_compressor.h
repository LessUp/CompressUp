#pragma once

#include "compressor.h"

namespace compressup {

// Delta编码: 存储相邻字节之间的差值
// 适用于有平滑变化的数据（如音频、图像等）
class DeltaCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;
};

} // namespace compressup
