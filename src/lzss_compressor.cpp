#include "lzss_compressor.h"

#include <algorithm>
#include <stdexcept>

namespace compressup {

std::string LzssCompressor::name() const {
    return "lzss";
}

std::pair<std::size_t, std::size_t> LzssCompressor::find_longest_match(
    std::string_view input, std::size_t pos) const {
    
    std::size_t best_offset = 0;
    std::size_t best_length = 0;
    
    std::size_t search_start = (pos > kWindowSize) ? pos - kWindowSize : 0;
    std::size_t max_match = std::min(kLookAheadSize, input.size() - pos);
    
    for (std::size_t i = search_start; i < pos; ++i) {
        std::size_t length = 0;
        while (length < max_match && input[i + length] == input[pos + length]) {
            ++length;
            // 允许超越当前位置的匹配（用于重复模式）
            if (i + length >= pos) {
                break;
            }
        }
        
        if (length > best_length) {
            best_length = length;
            best_offset = pos - i;
        }
    }
    
    return {best_offset, best_length};
}

std::vector<Byte> LzssCompressor::compress(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    
    std::vector<Byte> output;
    
    // 写入原始长度 (8字节, 小端序)
    std::uint64_t orig_len = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(orig_len >> (i * 8)));
    }
    
    std::vector<Byte> temp_flags;
    std::vector<Byte> temp_data;
    
    std::size_t pos = 0;
    Byte flag_byte = 0;
    int flag_bit = 0;
    
    while (pos < input.size()) {
        auto [offset, length] = find_longest_match(input, pos);
        
        if (length >= kMinMatchLength) {
            // 匹配: flag bit = 1
            flag_byte |= (1 << flag_bit);
            
            // 写入 offset (12 bits) 和 length - kMinMatchLength (4 bits) = 2 bytes
            std::uint16_t encoded = 
                (static_cast<std::uint16_t>(offset - 1) << 4) | 
                static_cast<std::uint16_t>(length - kMinMatchLength);
            temp_data.push_back(static_cast<Byte>(encoded >> 8));
            temp_data.push_back(static_cast<Byte>(encoded & 0xFF));
            
            pos += length;
        } else {
            // 字面量: flag bit = 0
            temp_data.push_back(static_cast<Byte>(input[pos]));
            ++pos;
        }
        
        ++flag_bit;
        if (flag_bit == 8) {
            temp_flags.push_back(flag_byte);
            flag_byte = 0;
            flag_bit = 0;
        }
    }
    
    // 写入最后的标志字节
    if (flag_bit > 0) {
        temp_flags.push_back(flag_byte);
    }
    
    // 写入标志数量
    std::uint32_t flag_count = static_cast<std::uint32_t>(temp_flags.size());
    for (int i = 0; i < 4; ++i) {
        output.push_back(static_cast<Byte>(flag_count >> (i * 8)));
    }
    
    // 写入标志字节
    output.insert(output.end(), temp_flags.begin(), temp_flags.end());
    
    // 写入数据
    output.insert(output.end(), temp_data.begin(), temp_data.end());
    
    return output;
}

std::string LzssCompressor::decompress(const std::vector<Byte>& input) {
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 12) {
        throw std::runtime_error("LZSS: input too short");
    }
    
    const Byte* data = input.data();
    const Byte* end = data + input.size();
    
    // 读取原始长度
    std::uint64_t orig_len = 0;
    for (int i = 0; i < 8; ++i) {
        orig_len |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    // 读取标志数量
    std::uint32_t flag_count = 0;
    for (int i = 0; i < 4; ++i) {
        flag_count |= static_cast<std::uint32_t>(*data++) << (i * 8);
    }
    
    if (data + flag_count > end) {
        throw std::runtime_error("LZSS: invalid flag count");
    }
    
    const Byte* flags = data;
    data += flag_count;
    
    std::string output;
    output.reserve(orig_len);
    
    std::size_t flag_index = 0;
    int flag_bit = 0;
    
    while (output.size() < orig_len && data < end) {
        if (flag_index >= flag_count) {
            throw std::runtime_error("LZSS: ran out of flags");
        }
        
        bool is_match = (flags[flag_index] >> flag_bit) & 1;
        
        if (is_match) {
            if (data + 2 > end) {
                throw std::runtime_error("LZSS: unexpected end of data");
            }
            
            std::uint16_t encoded = (static_cast<std::uint16_t>(data[0]) << 8) | data[1];
            data += 2;
            
            std::size_t offset = (encoded >> 4) + 1;
            std::size_t length = (encoded & 0x0F) + kMinMatchLength;
            
            if (offset > output.size()) {
                throw std::runtime_error("LZSS: invalid offset");
            }
            
            std::size_t start = output.size() - offset;
            for (std::size_t i = 0; i < length && output.size() < orig_len; ++i) {
                output.push_back(output[start + i]);
            }
        } else {
            if (data >= end) {
                throw std::runtime_error("LZSS: unexpected end of data");
            }
            output.push_back(static_cast<char>(*data++));
        }
        
        ++flag_bit;
        if (flag_bit == 8) {
            ++flag_index;
            flag_bit = 0;
        }
    }
    
    if (output.size() != orig_len) {
        throw std::runtime_error("LZSS: output size mismatch");
    }
    
    return output;
}

} // namespace compressup
