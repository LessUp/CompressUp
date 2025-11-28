#include "container.h"

#include <stdexcept>

namespace compressup {

namespace {

constexpr Byte kMagic = static_cast<Byte>(0xC3);
constexpr std::size_t kHeaderSize = 1 + 1 + 8;

AlgorithmId to_algorithm_id(Byte value) {
    switch (static_cast<AlgorithmId>(value)) {
    case AlgorithmId::Rle:
        return AlgorithmId::Rle;
    case AlgorithmId::Lz77:
        return AlgorithmId::Lz77;
    case AlgorithmId::Huffman:
        return AlgorithmId::Huffman;
    case AlgorithmId::Lzw:
        return AlgorithmId::Lzw;
    case AlgorithmId::Lzss:
        return AlgorithmId::Lzss;
    case AlgorithmId::Delta:
        return AlgorithmId::Delta;
    case AlgorithmId::Bwt:
        return AlgorithmId::Bwt;
    }

    throw std::runtime_error("Unknown algorithm id in container");
}

}

std::vector<Byte> pack_container(AlgorithmId algorithm,
                                 std::uint64_t original_size,
                                 const std::vector<Byte>& compressed) {
    std::vector<Byte> out;
    out.reserve(kHeaderSize + compressed.size());

    out.push_back(kMagic);
    out.push_back(static_cast<Byte>(algorithm));

    for (int i = 0; i < 8; ++i) {
        Byte b = static_cast<Byte>((original_size >> (i * 8)) & 0xFFu);
        out.push_back(b);
    }

    out.insert(out.end(), compressed.begin(), compressed.end());

    return out;
}

UnpackedContainer unpack_container(const std::vector<Byte>& data) {
    if (data.size() < kHeaderSize) {
        throw std::runtime_error("Container too small");
    }

    if (data[0] != kMagic) {
        throw std::runtime_error("Invalid container magic");
    }

    Byte algo_byte = data[1];
    AlgorithmId algorithm = to_algorithm_id(algo_byte);

    std::uint64_t original_size = 0;
    for (int i = 0; i < 8; ++i) {
        original_size |= static_cast<std::uint64_t>(data[2 + i]) << (i * 8);
    }

    std::vector<Byte> payload;
    payload.insert(payload.end(), data.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), data.end());

    UnpackedContainer result;
    result.algorithm = algorithm;
    result.original_size = original_size;
    result.payload = std::move(payload);

    return result;
}

} // namespace compressup
