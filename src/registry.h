#pragma once

#include "compressor.h"
#include "types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace compressup {

enum class AlgorithmId : std::uint8_t {
    Rle = 1,
    Lz77 = 2,
};

std::unique_ptr<ICompressor> create_compressor(const std::string& name);
std::unique_ptr<ICompressor> create_compressor(AlgorithmId id);
AlgorithmId algorithm_id_from_name(const std::string& name);
std::string algorithm_name_from_id(AlgorithmId id);
std::vector<std::string> available_algorithms();

} // namespace compressup
