#pragma once

#include "compressor.h"
#include "types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace compressup {

// 算法ID枚举
enum class AlgorithmId : std::uint8_t {
    Rle = 1,
    Lz77 = 2,
    Huffman = 3,
    Lzw = 4,
    Lzss = 5,
    Delta = 6,
    Bwt = 7,
};

// 算法信息结构
struct AlgorithmInfo {
    std::string name;
    std::string description;
    AlgorithmCategory category;
    AlgorithmId id;
};

// 工厂函数
std::unique_ptr<ICompressor> create_compressor(const std::string& name);
std::unique_ptr<ICompressor> create_compressor(AlgorithmId id);

// 名称和ID转换
AlgorithmId algorithm_id_from_name(const std::string& name);
std::string algorithm_name_from_id(AlgorithmId id);

// 查询所有可用算法
std::vector<std::string> available_algorithms();
std::vector<AlgorithmInfo> available_algorithm_infos();

// 按类别获取算法
std::vector<std::string> algorithms_by_category(AlgorithmCategory category);

} // namespace compressup
