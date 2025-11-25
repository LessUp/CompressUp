#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <functional>
#include <chrono>

namespace compressup {

// 基础类型
using Byte = std::uint8_t;
using ByteSpan = std::span<const Byte>;
using ByteVector = std::vector<Byte>;

// 压缩级别
enum class CompressionLevel : int {
    Fastest = 1,
    Fast = 3,
    Default = 6,
    Better = 9,
    Best = 12
};

// 算法类别
enum class AlgorithmCategory {
    Entropy,      // 熵编码: Huffman, Arithmetic
    Dictionary,   // 字典压缩: LZ77, LZW, LZSS
    Transform,    // 变换: BWT, MTF, Delta
    Hybrid        // 混合: Deflate (LZ77 + Huffman)
};

// 压缩统计信息
struct CompressionStats {
    std::size_t input_size{0};
    std::size_t output_size{0};
    std::chrono::microseconds duration{0};
    
    double ratio() const {
        return input_size > 0 ? 
            static_cast<double>(output_size) / static_cast<double>(input_size) : 0.0;
    }
    
    double throughput_mb_s() const {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        if (ms <= 0) return 0.0;
        return (static_cast<double>(input_size) / (1024.0 * 1024.0)) / (ms / 1000.0);
    }
};

// 进度回调
using ProgressCallback = std::function<void(std::size_t processed, std::size_t total)>;

} // namespace compressup
