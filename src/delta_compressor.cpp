#include "delta_compressor.h"

#include <stdexcept>

namespace compressup {

std::string DeltaCompressor::name() const {
    return "delta";
}

std::vector<Byte> DeltaCompressor::compress(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    
    std::vector<Byte> output;
    output.reserve(input.size() + 8);
    
    // 写入原始长度 (8字节, 小端序)
    std::uint64_t orig_len = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(orig_len >> (i * 8)));
    }
    
    // 第一个字节直接存储
    Byte prev = static_cast<Byte>(input[0]);
    output.push_back(prev);
    
    // 后续字节存储差值
    for (std::size_t i = 1; i < input.size(); ++i) {
        Byte curr = static_cast<Byte>(input[i]);
        // 差值会自动溢出到0-255范围
        output.push_back(static_cast<Byte>(curr - prev));
        prev = curr;
    }
    
    return output;
}

std::string DeltaCompressor::decompress(const std::vector<Byte>& input) {
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 9) {
        throw std::runtime_error("Delta: input too short");
    }
    
    const Byte* data = input.data();
    
    // 读取原始长度
    std::uint64_t orig_len = 0;
    for (int i = 0; i < 8; ++i) {
        orig_len |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    if (input.size() < 8 + orig_len) {
        throw std::runtime_error("Delta: input size mismatch");
    }
    
    std::string output;
    output.reserve(orig_len);
    
    // 第一个字节
    Byte prev = *data++;
    output.push_back(static_cast<char>(prev));
    
    // 还原后续字节
    for (std::size_t i = 1; i < orig_len; ++i) {
        Byte delta = *data++;
        Byte curr = static_cast<Byte>(prev + delta);
        output.push_back(static_cast<char>(curr));
        prev = curr;
    }
    
    return output;
}

} // namespace compressup
