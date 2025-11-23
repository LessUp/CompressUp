#include "api.h"

#include "container.h"
#include "file_io.h"
#include "registry.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace compressup {

void compress_file(const std::string& input_path,
                   const std::string& output_path,
                   const std::string& algorithm_name) {
    std::string text = read_text_file(input_path);

    AlgorithmId id = algorithm_id_from_name(algorithm_name);
    auto compressor = create_compressor(id);

    std::vector<Byte> compressed = compressor->compress(text);

    std::uint64_t original_size = static_cast<std::uint64_t>(text.size());
    std::vector<Byte> container = pack_container(id, original_size, compressed);

    write_binary_file(output_path, container);
}

void decompress_file(const std::string& input_path,
                     const std::string& output_path) {
    std::vector<Byte> data = read_binary_file(input_path);

    UnpackedContainer unpacked = unpack_container(data);

    auto compressor = create_compressor(unpacked.algorithm);
    std::string text = compressor->decompress(unpacked.payload);

    if (static_cast<std::uint64_t>(text.size()) != unpacked.original_size) {
        throw std::runtime_error("Decompressed size does not match original size");
    }

    write_text_file(output_path, text);
}

} // namespace compressup
