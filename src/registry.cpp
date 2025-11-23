#include "registry.h"

#include "lz77_compressor.h"
#include "rle_compressor.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace compressup {

std::unique_ptr<ICompressor> create_compressor(const std::string& name) {
    if (name == "rle") {
        return std::make_unique<RleCompressor>();
    }
    if (name == "lz77") {
        return std::make_unique<Lz77Compressor>();
    }

    throw std::invalid_argument("Unknown compressor: " + name);
}

std::unique_ptr<ICompressor> create_compressor(AlgorithmId id) {
    switch (id) {
    case AlgorithmId::Rle:
        return std::make_unique<RleCompressor>();
    case AlgorithmId::Lz77:
        return std::make_unique<Lz77Compressor>();
    }

    throw std::invalid_argument("Unknown AlgorithmId");
}

AlgorithmId algorithm_id_from_name(const std::string& name) {
    if (name == "rle") {
        return AlgorithmId::Rle;
    }
    if (name == "lz77") {
        return AlgorithmId::Lz77;
    }

    throw std::invalid_argument("Unknown algorithm name: " + name);
}

std::string algorithm_name_from_id(AlgorithmId id) {
    switch (id) {
    case AlgorithmId::Rle:
        return "rle";
    case AlgorithmId::Lz77:
        return "lz77";
    }

    throw std::invalid_argument("Unknown AlgorithmId");
}

std::vector<std::string> available_algorithms() {
    return {"rle", "lz77"};
}

} // namespace compressup
