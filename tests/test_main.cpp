#include "compressor.h"
#include "registry.h"

#include <iostream>
#include <string>
#include <vector>

using namespace compressup;

namespace {

bool check_roundtrip(const std::string& algo,
                     const std::string& name,
                     const std::string& input) {
    auto compressor = create_compressor(algo);
    std::vector<Byte> compressed = compressor->compress(input);
    std::string output = compressor->decompress(compressed);

    if (output != input) {
        std::cerr << "[FAIL] algo=" << algo
                  << " case=" << name
                  << " expected=\"" << input << "\""
                  << " got=\"" << output << "\"" << "\n";
        return false;
    }

    return true;
}

} // namespace

int main() {
    std::vector<std::string> algorithms = available_algorithms();

    std::vector<std::pair<std::string, std::string>> cases = {
        {"empty", ""},
        {"single-char", "a"},
        {"short", "abc"},
        {"repeated-a", "aaaaaa"},
        {"alternating", "abababababab"},
        {"sentence", "The quick brown fox jumps over the lazy dog"},
        {"repeated-block", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
        {"alnum", "0123456789abcdefghijklmnopqrstuvwxyz"},
    };

    int failed = 0;

    for (const auto& algo : algorithms) {
        for (const auto& c : cases) {
            if (!check_roundtrip(algo, c.first, c.second)) {
                ++failed;
            }
        }
    }

    if (failed == 0) {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }

    std::cout << failed << " tests failed" << std::endl;
    return 1;
}
