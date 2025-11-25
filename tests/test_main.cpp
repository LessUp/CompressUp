#include "compressor.h"
#include "registry.h"
#include "parallel_compressor.h"

#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace compressup;

namespace {

int g_passed = 0;
int g_failed = 0;

bool check_roundtrip(const std::string& algo,
                     const std::string& name,
                     const std::string& input) {
    try {
        auto compressor = create_compressor(algo);
        std::vector<Byte> compressed = compressor->compress(input);
        std::string output = compressor->decompress(compressed);

        if (output != input) {
            std::cerr << "[FAIL] algo=" << algo
                      << " case=" << name
                      << " input_len=" << input.size()
                      << " output_len=" << output.size() << "\n";
            ++g_failed;
            return false;
        }
        
        ++g_passed;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] algo=" << algo
                  << " case=" << name
                  << " exception: " << e.what() << "\n";
        ++g_failed;
        return false;
    }
}

std::string generate_random_string(std::size_t length, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<> dist(32, 126);  // 可打印ASCII
    
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<char>(dist(rng)));
    }
    return result;
}

std::string generate_binary_data(std::size_t length, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<> dist(0, 255);
    
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<char>(dist(rng)));
    }
    return result;
}

void test_algorithm_info() {
    std::cout << "\n=== Algorithm Info Test ===\n";
    
    auto infos = available_algorithm_infos();
    std::cout << "Available algorithms: " << infos.size() << "\n";
    
    for (const auto& info : infos) {
        std::cout << "  - " << info.name << ": " << info.description << "\n";
    }
    
    // 测试按类别查询
    auto entropy_algos = algorithms_by_category(AlgorithmCategory::Entropy);
    std::cout << "Entropy algorithms: " << entropy_algos.size() << "\n";
    
    auto dict_algos = algorithms_by_category(AlgorithmCategory::Dictionary);
    std::cout << "Dictionary algorithms: " << dict_algos.size() << "\n";
    
    auto transform_algos = algorithms_by_category(AlgorithmCategory::Transform);
    std::cout << "Transform algorithms: " << transform_algos.size() << "\n";
}

void test_parallel_compressor() {
    std::cout << "\n=== Parallel Compressor Test ===\n";
    
    std::string test_data = generate_random_string(100000);
    
    for (const auto& algo : available_algorithms()) {
        try {
            auto base = create_compressor(algo);
            ParallelCompressor parallel(std::move(base), 16 * 1024, 4);
            
            auto compressed = parallel.compress(test_data);
            auto decompressed = parallel.decompress(compressed);
            
            if (decompressed == test_data) {
                std::cout << "  [PASS] parallel_" << algo << "\n";
                ++g_passed;
            } else {
                std::cout << "  [FAIL] parallel_" << algo << "\n";
                ++g_failed;
            }
        } catch (const std::exception& e) {
            std::cout << "  [ERROR] parallel_" << algo << ": " << e.what() << "\n";
            ++g_failed;
        }
    }
}

} // namespace

int main() {
    std::cout << "CompressUp Test Suite\n";
    std::cout << "=====================\n\n";
    
    std::vector<std::string> algorithms = available_algorithms();
    std::cout << "Testing " << algorithms.size() << " algorithms\n";

    // 基础测试用例
    std::vector<std::pair<std::string, std::string>> basic_cases = {
        {"empty", ""},
        {"single-char", "a"},
        {"short", "abc"},
        {"repeated-a", "aaaaaa"},
        {"alternating", "abababababab"},
        {"sentence", "The quick brown fox jumps over the lazy dog"},
        {"repeated-block", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
        {"alnum", "0123456789abcdefghijklmnopqrstuvwxyz"},
    };

    std::cout << "\n=== Basic Roundtrip Tests ===\n";
    for (const auto& algo : algorithms) {
        int algo_passed = 0;
        for (const auto& c : basic_cases) {
            if (check_roundtrip(algo, c.first, c.second)) {
                ++algo_passed;
            }
        }
        std::cout << "  " << algo << ": " << algo_passed << "/" 
                  << basic_cases.size() << " passed\n";
    }

    // 长字符串测试
    std::cout << "\n=== Long String Tests ===\n";
    std::vector<std::pair<std::string, std::string>> long_cases = {
        {"random-1k", generate_random_string(1024)},
        {"random-10k", generate_random_string(10240)},
        {"binary-1k", generate_binary_data(1024)},
        {"repeated-long", std::string(5000, 'X')},
        {"pattern-long", []() {
            std::string s;
            for (int i = 0; i < 1000; ++i) s += "Hello World! ";
            return s;
        }()},
    };
    
    for (const auto& algo : algorithms) {
        int algo_passed = 0;
        for (const auto& c : long_cases) {
            if (check_roundtrip(algo, c.first, c.second)) {
                ++algo_passed;
            }
        }
        std::cout << "  " << algo << ": " << algo_passed << "/" 
                  << long_cases.size() << " passed\n";
    }

    // 算法信息测试
    test_algorithm_info();
    
    // 并行压缩测试
    test_parallel_compressor();

    // 总结
    std::cout << "\n=== Summary ===\n";
    std::cout << "Total passed: " << g_passed << "\n";
    std::cout << "Total failed: " << g_failed << "\n";

    if (g_failed == 0) {
        std::cout << "\nAll tests passed!\n";
        return 0;
    }

    std::cout << "\nSome tests failed.\n";
    return 1;
}
