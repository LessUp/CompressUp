#include "registry.h"

#include "bwt_compressor.h"
#include "delta_compressor.h"
#include "huffman_compressor.h"
#include "lz77_compressor.h"
#include "lzss_compressor.h"
#include "lzw_compressor.h"
#include "rle_compressor.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace compressup {

namespace {

// 算法信息表
const std::vector<AlgorithmInfo> kAlgorithmInfos = {
    {"rle", "Run-Length Encoding - 游程编码", AlgorithmCategory::Dictionary, AlgorithmId::Rle},
    {"lz77", "LZ77 - 滑动窗口字典压缩", AlgorithmCategory::Dictionary, AlgorithmId::Lz77},
    {"huffman", "Huffman Coding - 哈夫曼编码", AlgorithmCategory::Entropy, AlgorithmId::Huffman},
    {"lzw", "LZW - Lempel-Ziv-Welch字典压缩", AlgorithmCategory::Dictionary, AlgorithmId::Lzw},
    {"lzss", "LZSS - LZ77的优化变体", AlgorithmCategory::Dictionary, AlgorithmId::Lzss},
    {"delta", "Delta Encoding - 差分编码", AlgorithmCategory::Transform, AlgorithmId::Delta},
    {"bwt", "BWT+MTF - Burrows-Wheeler变换", AlgorithmCategory::Transform, AlgorithmId::Bwt},
};

} // namespace

std::unique_ptr<ICompressor> create_compressor(const std::string& name) {
    if (name == "rle") {
        return std::make_unique<RleCompressor>();
    }
    if (name == "lz77") {
        return std::make_unique<Lz77Compressor>();
    }
    if (name == "huffman") {
        return std::make_unique<HuffmanCompressor>();
    }
    if (name == "lzw") {
        return std::make_unique<LzwCompressor>();
    }
    if (name == "lzss") {
        return std::make_unique<LzssCompressor>();
    }
    if (name == "delta") {
        return std::make_unique<DeltaCompressor>();
    }
    if (name == "bwt") {
        return std::make_unique<BwtCompressor>();
    }

    throw std::invalid_argument("Unknown compressor: " + name);
}

std::unique_ptr<ICompressor> create_compressor(AlgorithmId id) {
    switch (id) {
    case AlgorithmId::Rle:
        return std::make_unique<RleCompressor>();
    case AlgorithmId::Lz77:
        return std::make_unique<Lz77Compressor>();
    case AlgorithmId::Huffman:
        return std::make_unique<HuffmanCompressor>();
    case AlgorithmId::Lzw:
        return std::make_unique<LzwCompressor>();
    case AlgorithmId::Lzss:
        return std::make_unique<LzssCompressor>();
    case AlgorithmId::Delta:
        return std::make_unique<DeltaCompressor>();
    case AlgorithmId::Bwt:
        return std::make_unique<BwtCompressor>();
    }

    throw std::invalid_argument("Unknown AlgorithmId");
}

AlgorithmId algorithm_id_from_name(const std::string& name) {
    if (name == "rle") return AlgorithmId::Rle;
    if (name == "lz77") return AlgorithmId::Lz77;
    if (name == "huffman") return AlgorithmId::Huffman;
    if (name == "lzw") return AlgorithmId::Lzw;
    if (name == "lzss") return AlgorithmId::Lzss;
    if (name == "delta") return AlgorithmId::Delta;
    if (name == "bwt") return AlgorithmId::Bwt;

    throw std::invalid_argument("Unknown algorithm name: " + name);
}

std::string algorithm_name_from_id(AlgorithmId id) {
    switch (id) {
    case AlgorithmId::Rle: return "rle";
    case AlgorithmId::Lz77: return "lz77";
    case AlgorithmId::Huffman: return "huffman";
    case AlgorithmId::Lzw: return "lzw";
    case AlgorithmId::Lzss: return "lzss";
    case AlgorithmId::Delta: return "delta";
    case AlgorithmId::Bwt: return "bwt";
    }

    throw std::invalid_argument("Unknown AlgorithmId");
}

std::vector<std::string> available_algorithms() {
    std::vector<std::string> result;
    result.reserve(kAlgorithmInfos.size());
    for (const auto& info : kAlgorithmInfos) {
        result.push_back(info.name);
    }
    return result;
}

std::vector<AlgorithmInfo> available_algorithm_infos() {
    return kAlgorithmInfos;
}

std::vector<std::string> algorithms_by_category(AlgorithmCategory category) {
    std::vector<std::string> result;
    for (const auto& info : kAlgorithmInfos) {
        if (info.category == category) {
            result.push_back(info.name);
        }
    }
    return result;
}

} // namespace compressup
