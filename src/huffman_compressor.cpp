#include "huffman_compressor.h"

#include <algorithm>
#include <stdexcept>

namespace compressup {

std::string HuffmanCompressor::name() const {
    return "huffman";
}

std::array<std::size_t, 256> HuffmanCompressor::build_frequency_table(std::string_view input) const {
    std::array<std::size_t, 256> freq{};
    for (unsigned char c : input) {
        ++freq[c];
    }
    return freq;
}

std::unique_ptr<HuffmanCompressor::Node> HuffmanCompressor::build_tree(
    const std::array<std::size_t, 256>& freq) const {
    
    auto cmp = [](const Node* a, const Node* b) {
        return a->freq > b->freq;
    };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> pq(cmp);
    
    std::vector<std::unique_ptr<Node>> nodes;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            auto node = std::make_unique<Node>();
            node->byte = static_cast<Byte>(i);
            node->freq = freq[i];
            pq.push(node.get());
            nodes.push_back(std::move(node));
        }
    }
    
    if (pq.empty()) {
        return nullptr;
    }
    
    // 只有一个符号的特殊情况
    if (pq.size() == 1) {
        auto root = std::make_unique<Node>();
        root->freq = pq.top()->freq;
        for (auto& n : nodes) {
            if (n.get() == pq.top()) {
                root->left = std::move(n);
                break;
            }
        }
        return root;
    }
    
    while (pq.size() > 1) {
        Node* left_ptr = pq.top(); pq.pop();
        Node* right_ptr = pq.top(); pq.pop();
        
        auto parent = std::make_unique<Node>();
        parent->freq = left_ptr->freq + right_ptr->freq;
        
        for (auto& n : nodes) {
            if (n.get() == left_ptr) {
                parent->left = std::move(n);
            } else if (n.get() == right_ptr) {
                parent->right = std::move(n);
            }
        }
        
        pq.push(parent.get());
        nodes.push_back(std::move(parent));
    }
    
    for (auto& n : nodes) {
        if (n.get() == pq.top()) {
            return std::move(n);
        }
    }
    
    return nullptr;
}

void HuffmanCompressor::generate_codes(
    const Node* node,
    std::vector<bool>& current_code,
    std::unordered_map<Byte, std::vector<bool>>& codes) const {
    
    if (!node) return;
    
    if (node->is_leaf()) {
        codes[node->byte] = current_code.empty() ? std::vector<bool>{false} : current_code;
        return;
    }
    
    current_code.push_back(false);
    generate_codes(node->left.get(), current_code, codes);
    current_code.pop_back();
    
    current_code.push_back(true);
    generate_codes(node->right.get(), current_code, codes);
    current_code.pop_back();
}

void HuffmanCompressor::serialize_tree(const Node* node, std::vector<Byte>& output) const {
    if (!node) {
        output.push_back(2);  // 空节点标记
        return;
    }
    
    if (node->is_leaf()) {
        output.push_back(1);  // 叶子节点标记
        output.push_back(node->byte);
    } else {
        output.push_back(0);  // 内部节点标记
        serialize_tree(node->left.get(), output);
        serialize_tree(node->right.get(), output);
    }
}

std::unique_ptr<HuffmanCompressor::Node> HuffmanCompressor::deserialize_tree(
    const Byte*& data, const Byte* end) const {
    
    if (data >= end) {
        throw std::runtime_error("Huffman: invalid tree data");
    }
    
    Byte marker = *data++;
    
    if (marker == 2) {
        // 空节点
        return nullptr;
    }
    
    auto node = std::make_unique<Node>();
    
    if (marker == 1) {
        // 叶子节点
        if (data >= end) {
            throw std::runtime_error("Huffman: incomplete leaf node");
        }
        node->byte = *data++;
    } else {
        // 内部节点
        node->left = deserialize_tree(data, end);
        node->right = deserialize_tree(data, end);
    }
    
    return node;
}

std::vector<Byte> HuffmanCompressor::compress(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    
    // 构建频率表和Huffman树
    auto freq = build_frequency_table(input);
    auto tree = build_tree(freq);
    
    // 生成编码表
    std::unordered_map<Byte, std::vector<bool>> codes;
    std::vector<bool> current_code;
    generate_codes(tree.get(), current_code, codes);
    
    std::vector<Byte> output;
    
    // 写入原始长度 (8字节, 小端序)
    std::uint64_t orig_len = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(orig_len >> (i * 8)));
    }
    
    // 序列化树
    std::vector<Byte> tree_data;
    serialize_tree(tree.get(), tree_data);
    
    // 写入树长度 (4字节, 小端序)
    std::uint32_t tree_len = static_cast<std::uint32_t>(tree_data.size());
    for (int i = 0; i < 4; ++i) {
        output.push_back(static_cast<Byte>(tree_len >> (i * 8)));
    }
    
    // 写入树数据
    output.insert(output.end(), tree_data.begin(), tree_data.end());
    
    // 编码数据
    std::vector<bool> bits;
    for (unsigned char c : input) {
        const auto& code = codes[c];
        bits.insert(bits.end(), code.begin(), code.end());
    }
    
    // 写入位数据的位数 (用于处理最后一个字节的padding)
    std::uint64_t bit_count = bits.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(bit_count >> (i * 8)));
    }
    
    // 将bits打包为字节
    for (std::size_t i = 0; i < bits.size(); i += 8) {
        Byte byte = 0;
        for (std::size_t j = 0; j < 8 && i + j < bits.size(); ++j) {
            if (bits[i + j]) {
                byte |= (1 << (7 - j));
            }
        }
        output.push_back(byte);
    }
    
    return output;
}

std::string HuffmanCompressor::decompress(const std::vector<Byte>& input) {
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 20) {
        throw std::runtime_error("Huffman: input too short");
    }
    
    const Byte* data = input.data();
    const Byte* end = data + input.size();
    
    // 读取原始长度
    std::uint64_t orig_len = 0;
    for (int i = 0; i < 8; ++i) {
        orig_len |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    // 读取树长度
    std::uint32_t tree_len = 0;
    for (int i = 0; i < 4; ++i) {
        tree_len |= static_cast<std::uint32_t>(*data++) << (i * 8);
    }
    
    if (data + tree_len + 8 > end) {
        throw std::runtime_error("Huffman: invalid tree length");
    }
    
    // 反序列化树
    const Byte* tree_start = data;
    auto tree = deserialize_tree(data, data + tree_len);
    data = tree_start + tree_len;
    
    // 读取位数
    std::uint64_t bit_count = 0;
    for (int i = 0; i < 8; ++i) {
        bit_count |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    // 解码
    std::string output;
    output.reserve(orig_len);
    
    const Node* current = tree.get();
    std::uint64_t bits_read = 0;
    
    while (data < end && output.size() < orig_len) {
        Byte byte = *data++;
        for (int i = 7; i >= 0 && bits_read < bit_count && output.size() < orig_len; --i, ++bits_read) {
            bool bit = (byte >> i) & 1;
            
            if (bit) {
                current = current->right.get();
            } else {
                current = current->left.get();
            }
            
            if (!current) {
                throw std::runtime_error("Huffman: invalid encoded data");
            }
            
            if (current->is_leaf()) {
                output.push_back(static_cast<char>(current->byte));
                current = tree.get();
            }
        }
    }
    
    if (output.size() != orig_len) {
        throw std::runtime_error("Huffman: output size mismatch");
    }
    
    return output;
}

} // namespace compressup
