#include "lzw_compressor.h"

#include <stdexcept>

namespace compressup {

std::string LzwCompressor::name() const {
    return "lzw";
}

void LzwCompressor::write_code(std::vector<Byte>& output, std::uint16_t code,
                               int& bit_buffer, int& bits_in_buffer) const {
    bit_buffer = (bit_buffer << kCodeBits) | code;
    bits_in_buffer += kCodeBits;
    
    while (bits_in_buffer >= 8) {
        bits_in_buffer -= 8;
        output.push_back(static_cast<Byte>((bit_buffer >> bits_in_buffer) & 0xFF));
    }
}

std::uint16_t LzwCompressor::read_code(const Byte*& data, const Byte* end,
                                       int& bit_buffer, int& bits_in_buffer) const {
    while (bits_in_buffer < kCodeBits) {
        if (data >= end) {
            throw std::runtime_error("LZW: unexpected end of data");
        }
        bit_buffer = (bit_buffer << 8) | *data++;
        bits_in_buffer += 8;
    }
    
    bits_in_buffer -= kCodeBits;
    return (bit_buffer >> bits_in_buffer) & ((1 << kCodeBits) - 1);
}

std::vector<Byte> LzwCompressor::compress(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    
    std::vector<Byte> output;
    
    // 写入原始长度 (8字节, 小端序)
    std::uint64_t orig_len = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(orig_len >> (i * 8)));
    }
    
    // 初始化字典
    std::unordered_map<std::string, std::uint16_t> dictionary;
    for (std::size_t i = 0; i < kInitialDictSize; ++i) {
        dictionary[std::string(1, static_cast<char>(i))] = static_cast<std::uint16_t>(i);
    }
    
    std::uint16_t next_code = kInitialDictSize;
    std::string current;
    
    int bit_buffer = 0;
    int bits_in_buffer = 0;
    
    for (char c : input) {
        std::string next_str = current + c;
        
        if (dictionary.count(next_str)) {
            current = next_str;
        } else {
            // 输出当前字符串的编码
            write_code(output, dictionary[current], bit_buffer, bits_in_buffer);
            
            // 添加新字符串到字典
            if (next_code < kMaxDictSize) {
                dictionary[next_str] = next_code++;
            }
            
            current = std::string(1, c);
        }
    }
    
    // 输出最后的字符串
    if (!current.empty()) {
        write_code(output, dictionary[current], bit_buffer, bits_in_buffer);
    }
    
    // 刷新剩余的位
    if (bits_in_buffer > 0) {
        output.push_back(static_cast<Byte>((bit_buffer << (8 - bits_in_buffer)) & 0xFF));
    }
    
    return output;
}

std::string LzwCompressor::decompress(const std::vector<Byte>& input) {
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 8) {
        throw std::runtime_error("LZW: input too short");
    }
    
    const Byte* data = input.data();
    const Byte* end = data + input.size();
    
    // 读取原始长度
    std::uint64_t orig_len = 0;
    for (int i = 0; i < 8; ++i) {
        orig_len |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    // 初始化字典
    std::vector<std::string> dictionary;
    dictionary.reserve(kMaxDictSize);
    for (std::size_t i = 0; i < kInitialDictSize; ++i) {
        dictionary.emplace_back(1, static_cast<char>(i));
    }
    
    int bit_buffer = 0;
    int bits_in_buffer = 0;
    
    std::string output;
    output.reserve(orig_len);
    
    // 读取第一个编码
    std::uint16_t old_code = read_code(data, end, bit_buffer, bits_in_buffer);
    if (old_code >= dictionary.size()) {
        throw std::runtime_error("LZW: invalid first code");
    }
    
    output += dictionary[old_code];
    std::string s = dictionary[old_code];
    
    while (output.size() < orig_len) {
        std::uint16_t new_code;
        try {
            new_code = read_code(data, end, bit_buffer, bits_in_buffer);
        } catch (...) {
            break;
        }
        
        std::string entry;
        if (new_code < dictionary.size()) {
            entry = dictionary[new_code];
        } else if (new_code == dictionary.size()) {
            // 特殊情况: cScSc
            entry = s + s[0];
        } else {
            throw std::runtime_error("LZW: invalid code");
        }
        
        output += entry;
        
        // 添加新条目到字典
        if (dictionary.size() < kMaxDictSize) {
            dictionary.push_back(s + entry[0]);
        }
        
        s = entry;
    }
    
    // 截断到原始长度
    if (output.size() > orig_len) {
        output.resize(orig_len);
    }
    
    return output;
}

} // namespace compressup
