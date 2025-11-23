#include "rle_compressor.h"

#include <stdexcept>

namespace compressup {

std::string RleCompressor::name() const {
    return "rle";
}

std::vector<Byte> RleCompressor::compress(std::string_view input) {
    std::vector<Byte> out;
    if (input.empty()) {
        return out;
    }

    out.reserve(input.size());

    std::size_t i = 0;
    while (i < input.size()) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        std::size_t run_length = 1;
        while (i + run_length < input.size() &&
               static_cast<unsigned char>(input[i + run_length]) == ch &&
               run_length < 255) {
            ++run_length;
        }

        out.push_back(static_cast<Byte>(run_length));
        out.push_back(static_cast<Byte>(ch));

        i += run_length;
    }

    return out;
}

std::string RleCompressor::decompress(const std::vector<Byte>& input) {
    if (input.size() % 2 != 0) {
        throw std::runtime_error("RLE compressed data size must be even");
    }

    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); i += 2) {
        unsigned int count = static_cast<unsigned int>(input[i]);
        unsigned char ch = static_cast<unsigned char>(input[i + 1]);
        output.append(count, static_cast<char>(ch));
    }

    return output;
}

} // namespace compressup
