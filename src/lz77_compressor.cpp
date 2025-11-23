#include "lz77_compressor.h"

#include <stdexcept>

namespace compressup {

std::string Lz77Compressor::name() const {
    return "lz77";
}

std::vector<Byte> Lz77Compressor::compress(std::string_view input) {
    std::vector<Byte> out;
    const std::size_t n = input.size();
    if (n == 0) {
        return out;
    }

    out.reserve(n);

    std::size_t pos = 0;
    while (pos < n) {
        std::size_t bestLen = 0;
        std::size_t bestOffset = 0;

        const std::size_t windowStart = (pos > kWindowSize) ? (pos - kWindowSize) : 0;

        for (std::size_t candidate = windowStart; candidate < pos; ++candidate) {
            std::size_t length = 0;
            while (length < kMaxMatchLength &&
                   pos + length < n &&
                   input[candidate + length] == input[pos + length]) {
                ++length;
            }

            if (length > bestLen && length >= 3) {
                bestLen = length;
                bestOffset = pos - candidate;
                if (bestLen == kMaxMatchLength) {
                    break;
                }
            }
        }

        if (bestLen >= 3 && bestOffset > 0 && bestOffset <= 0xFFFF) {
            Byte lengthByte = static_cast<Byte>(bestLen); // high bit 0 => match token
            Byte offsetHigh = static_cast<Byte>((bestOffset >> 8) & 0xFF);
            Byte offsetLow = static_cast<Byte>(bestOffset & 0xFF);

            out.push_back(lengthByte);
            out.push_back(offsetHigh);
            out.push_back(offsetLow);

            pos += bestLen;
        } else {
            Byte flag = static_cast<Byte>(0x80); // literal token
            Byte ch = static_cast<Byte>(static_cast<unsigned char>(input[pos]));
            out.push_back(flag);
            out.push_back(ch);
            ++pos;
        }
    }

    return out;
}

std::string Lz77Compressor::decompress(const std::vector<Byte>& input) {
    std::string output;
    output.reserve(input.size());

    std::size_t pos = 0;
    const std::size_t n = input.size();

    while (pos < n) {
        if (pos >= n) {
            throw std::runtime_error("Unexpected end of input in LZ77 stream");
        }

        Byte token = input[pos++];

        if (token & 0x80u) {
            // literal
            if (pos >= n) {
                throw std::runtime_error("LZ77 literal token missing byte");
            }
            Byte ch = input[pos++];
            output.push_back(static_cast<char>(ch));
        } else {
            // match: token = length (3..kMaxMatchLength)
            std::size_t length = static_cast<std::size_t>(token);
            if (length < 3 || length > kMaxMatchLength) {
                throw std::runtime_error("Invalid LZ77 match length");
            }
            if (pos + 1 >= n) {
                throw std::runtime_error("LZ77 match token missing offset bytes");
            }
            std::size_t offsetHigh = static_cast<std::size_t>(input[pos++]);
            std::size_t offsetLow = static_cast<std::size_t>(input[pos++]);
            std::size_t offset = (offsetHigh << 8) | offsetLow;
            if (offset == 0 || offset > output.size()) {
                throw std::runtime_error("Invalid LZ77 match offset");
            }

            std::size_t start = output.size() - offset;
            for (std::size_t i = 0; i < length; ++i) {
                char c = output[start + i];
                output.push_back(c);
            }
        }
    }

    return output;
}

} // namespace compressup
