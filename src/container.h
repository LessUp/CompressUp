#pragma once

#include "registry.h"
#include "types.h"

#include <cstdint>
#include <vector>

namespace compressup {

struct ContainerHeader {
    AlgorithmId algorithm;
    std::uint64_t original_size;
};

std::vector<Byte> pack_container(AlgorithmId algorithm,
                                 std::uint64_t original_size,
                                 const std::vector<Byte>& compressed);

struct UnpackedContainer {
    AlgorithmId algorithm;
    std::uint64_t original_size;
    std::vector<Byte> payload;
};

UnpackedContainer unpack_container(const std::vector<Byte>& data);

} // namespace compressup
