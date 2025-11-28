#include "bwt_compressor.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace compressup {

std::string BwtCompressor::name() const {
    return "bwt";
}

std::pair<std::string, std::size_t> BwtCompressor::bwt_transform(std::string_view input) const {
    if (input.empty()) {
        return {"", 0};
    }
    
    std::size_t n = input.size();
    
    // 创建后缀数组
    std::vector<std::size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    
    // 按循环移位后的字符串排序
    std::sort(indices.begin(), indices.end(), [&input, n](std::size_t a, std::size_t b) {
        for (std::size_t i = 0; i < n; ++i) {
            unsigned char ca = static_cast<unsigned char>(input[(a + i) % n]);
            unsigned char cb = static_cast<unsigned char>(input[(b + i) % n]);
            if (ca != cb) {
                return ca < cb;
            }
        }
        return false;
    });
    
    // 构建BWT输出：每个排序位置的前一个字符
    std::string output;
    output.reserve(n);
    std::size_t primary_index = 0;
    
    for (std::size_t i = 0; i < n; ++i) {
        if (indices[i] == 0) {
            primary_index = i;
            output.push_back(input[n - 1]);
        } else {
            output.push_back(input[indices[i] - 1]);
        }
    }
    
    return {output, primary_index};
}

std::string BwtCompressor::bwt_inverse(std::string_view input, std::size_t primary_index) const {
    if (input.empty()) {
        return "";
    }
    
    std::size_t n = input.size();
    
    if (primary_index >= n) {
        throw std::runtime_error("BWT: invalid primary index");
    }
    
    // 计数排序：统计每个字符出现次数
    std::array<std::size_t, 256> count{};
    for (unsigned char c : input) {
        ++count[c];
    }
    
    // 计算每个字符在第一列的起始位置
    std::array<std::size_t, 256> cumsum{};
    std::size_t sum = 0;
    for (int c = 0; c < 256; ++c) {
        cumsum[c] = sum;
        sum += count[c];
    }
    
    // 构建LF映射：对于最后一列位置i的字符c，它在第一列的位置
    // LF[i] = cumsum[c] + (c在位置i之前在最后一列出现的次数)
    std::vector<std::size_t> lf(n);
    std::array<std::size_t, 256> seen{};
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        lf[i] = cumsum[c] + seen[c];
        ++seen[c];
    }
    
    // 逆变换：从primary_index开始，向后遍历
    std::string output(n, '\0');
    std::size_t idx = primary_index;
    for (std::size_t i = n; i > 0; --i) {
        output[i - 1] = input[idx];
        idx = lf[idx];
    }
    
    return output;
}

std::vector<Byte> BwtCompressor::mtf_encode(std::string_view input) const {
    // 初始化字母表
    std::vector<Byte> alphabet(256);
    std::iota(alphabet.begin(), alphabet.end(), 0);
    
    std::vector<Byte> output;
    output.reserve(input.size());
    
    for (unsigned char c : input) {
        // 查找字符位置
        auto it = std::find(alphabet.begin(), alphabet.end(), c);
        std::size_t pos = static_cast<std::size_t>(it - alphabet.begin());
        output.push_back(static_cast<Byte>(pos));
        
        // 将字符移到前面
        if (pos > 0) {
            alphabet.erase(it);
            alphabet.insert(alphabet.begin(), c);
        }
    }
    
    return output;
}

std::string BwtCompressor::mtf_decode(const std::vector<Byte>& input) const {
    // 初始化字母表
    std::vector<Byte> alphabet(256);
    std::iota(alphabet.begin(), alphabet.end(), 0);
    
    std::string output;
    output.reserve(input.size());
    
    for (Byte pos : input) {
        Byte c = alphabet[pos];
        output.push_back(static_cast<char>(c));
        
        // 将字符移到前面
        if (pos > 0) {
            alphabet.erase(alphabet.begin() + pos);
            alphabet.insert(alphabet.begin(), c);
        }
    }
    
    return output;
}

std::vector<Byte> BwtCompressor::compress(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    
    // 限制块大小
    std::size_t block_size = std::min(input.size(), kMaxBlockSize);
    
    std::vector<Byte> output;
    
    // 写入原始长度 (8字节, 小端序)
    std::uint64_t orig_len = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(orig_len >> (i * 8)));
    }
    
    std::size_t pos = 0;
    while (pos < input.size()) {
        std::size_t chunk_size = std::min(block_size, input.size() - pos);
        std::string_view chunk = input.substr(pos, chunk_size);
        
        // BWT变换
        auto [bwt_output, primary_index] = bwt_transform(chunk);
        
        // 写入primary_index (8字节)
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<Byte>(primary_index >> (i * 8)));
        }
        
        // 写入块大小 (8字节)
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<Byte>(chunk_size >> (i * 8)));
        }
        
        // MTF编码
        auto mtf_output = mtf_encode(bwt_output);
        
        // 写入MTF数据
        output.insert(output.end(), mtf_output.begin(), mtf_output.end());
        
        pos += chunk_size;
    }
    
    return output;
}

std::string BwtCompressor::decompress(const std::vector<Byte>& input) {
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 8) {
        throw std::runtime_error("BWT: input too short");
    }
    
    const Byte* data = input.data();
    const Byte* end = data + input.size();
    
    // 读取原始长度
    std::uint64_t orig_len = 0;
    for (int i = 0; i < 8; ++i) {
        orig_len |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    std::string output;
    output.reserve(orig_len);
    
    while (output.size() < orig_len && data + 16 <= end) {
        // 读取primary_index
        std::uint64_t primary_index = 0;
        for (int i = 0; i < 8; ++i) {
            primary_index |= static_cast<std::uint64_t>(*data++) << (i * 8);
        }
        
        // 读取块大小
        std::uint64_t chunk_size = 0;
        for (int i = 0; i < 8; ++i) {
            chunk_size |= static_cast<std::uint64_t>(*data++) << (i * 8);
        }
        
        if (data + chunk_size > end) {
            throw std::runtime_error("BWT: invalid chunk size");
        }
        
        // MTF解码
        std::vector<Byte> mtf_data(data, data + chunk_size);
        data += chunk_size;
        
        std::string bwt_data = mtf_decode(mtf_data);
        
        // BWT逆变换
        std::string decoded = bwt_inverse(bwt_data, primary_index);
        output += decoded;
    }
    
    if (output.size() != orig_len) {
        throw std::runtime_error("BWT: output size mismatch");
    }
    
    return output;
}

} // namespace compressup
