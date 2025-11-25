#pragma once

#include "compressor.h"

#include <array>
#include <memory>
#include <queue>
#include <unordered_map>

namespace compressup {

class HuffmanCompressor : public ICompressor {
public:
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;

private:
    // Huffman树节点
    struct Node {
        Byte byte{0};
        std::size_t freq{0};
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        
        bool is_leaf() const { return !left && !right; }
    };
    
    // 构建频率表
    std::array<std::size_t, 256> build_frequency_table(std::string_view input) const;
    
    // 构建Huffman树
    std::unique_ptr<Node> build_tree(const std::array<std::size_t, 256>& freq) const;
    
    // 生成编码表
    void generate_codes(const Node* node, 
                       std::vector<bool>& current_code,
                       std::unordered_map<Byte, std::vector<bool>>& codes) const;
    
    // 序列化树结构
    void serialize_tree(const Node* node, std::vector<Byte>& output) const;
    
    // 反序列化树结构
    std::unique_ptr<Node> deserialize_tree(const Byte*& data, const Byte* end) const;
};

} // namespace compressup
