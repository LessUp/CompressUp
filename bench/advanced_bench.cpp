#include "file_io.h"
#include "registry.h"
#include "parallel_compressor.h"
#include "advanced_io.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>

using namespace compressup;

namespace {

// 详细的Benchmark结果
struct DetailedBenchResult {
    std::string algorithm;
    std::string test_name;
    std::size_t original_size{};
    std::size_t compressed_size{};
    double ratio{};
    
    // 时间统计 (毫秒)
    double compress_min_ms{};
    double compress_max_ms{};
    double compress_avg_ms{};
    double compress_std_ms{};
    
    double decompress_min_ms{};
    double decompress_max_ms{};
    double decompress_avg_ms{};
    double decompress_std_ms{};
    
    // 吞吐量 (MB/s)
    double compress_throughput{};
    double decompress_throughput{};
    
    // 验证结果
    bool verified{false};
};

// 计算统计信息
struct Stats {
    double min{}, max{}, avg{}, std_dev{};
};

Stats calculate_stats(const std::vector<double>& values) {
    Stats s;
    if (values.empty()) return s;
    
    s.min = *std::min_element(values.begin(), values.end());
    s.max = *std::max_element(values.begin(), values.end());
    s.avg = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    double sq_sum = 0.0;
    for (double v : values) {
        sq_sum += (v - s.avg) * (v - s.avg);
    }
    s.std_dev = std::sqrt(sq_sum / values.size());
    
    return s;
}

// 生成测试数据
std::string generate_test_data(const std::string& type, std::size_t size) {
    std::string data;
    data.reserve(size);
    
    std::mt19937 rng(42);  // 固定种子保证可重复
    
    if (type == "random") {
        // 纯随机数据（最难压缩）
        std::uniform_int_distribution<> dist(0, 255);
        for (std::size_t i = 0; i < size; ++i) {
            data.push_back(static_cast<char>(dist(rng)));
        }
    } else if (type == "text") {
        // 模拟英文文本
        const char* words[] = {
            "the ", "be ", "to ", "of ", "and ", "a ", "in ", "that ", "have ", "I ",
            "it ", "for ", "not ", "on ", "with ", "he ", "as ", "you ", "do ", "at ",
            "this ", "but ", "his ", "by ", "from ", "they ", "we ", "say ", "her ", "she "
        };
        while (data.size() < size) {
            data += words[rng() % 30];
        }
        data.resize(size);
    } else if (type == "repetitive") {
        // 高重复数据
        std::string pattern = "ABCDEFGHIJ";
        while (data.size() < size) {
            data += pattern;
        }
        data.resize(size);
    } else if (type == "binary") {
        // 模拟二进制数据（有一定规律）
        for (std::size_t i = 0; i < size; ++i) {
            data.push_back(static_cast<char>((i * 7 + i / 256) % 256));
        }
    } else if (type == "sparse") {
        // 稀疏数据（大量零）
        std::uniform_int_distribution<> dist(0, 10);
        for (std::size_t i = 0; i < size; ++i) {
            data.push_back(static_cast<char>(dist(rng) == 0 ? rng() % 256 : 0));
        }
    }
    
    return data;
}

// 运行单个benchmark
DetailedBenchResult run_detailed_bench(
    const std::string& algorithm,
    const std::string& test_name,
    const std::string& data,
    int warmup_runs,
    int measurement_runs) {
    
    DetailedBenchResult result;
    result.algorithm = algorithm;
    result.test_name = test_name;
    result.original_size = data.size();
    
    auto compressor = create_compressor(algorithm);
    
    // Warmup
    std::vector<Byte> compressed;
    for (int i = 0; i < warmup_runs; ++i) {
        compressed = compressor->compress(data);
    }
    
    result.compressed_size = compressed.size();
    result.ratio = static_cast<double>(result.compressed_size) / 
                   static_cast<double>(result.original_size);
    
    // 测量压缩时间
    std::vector<double> compress_times;
    for (int i = 0; i < measurement_runs; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto tmp = compressor->compress(data);
        auto end = std::chrono::steady_clock::now();
        
        std::chrono::duration<double, std::milli> diff = end - start;
        compress_times.push_back(diff.count());
    }
    
    auto compress_stats = calculate_stats(compress_times);
    result.compress_min_ms = compress_stats.min;
    result.compress_max_ms = compress_stats.max;
    result.compress_avg_ms = compress_stats.avg;
    result.compress_std_ms = compress_stats.std_dev;
    
    // 测量解压时间
    std::vector<double> decompress_times;
    std::string decompressed;
    for (int i = 0; i < measurement_runs; ++i) {
        auto start = std::chrono::steady_clock::now();
        decompressed = compressor->decompress(compressed);
        auto end = std::chrono::steady_clock::now();
        
        std::chrono::duration<double, std::milli> diff = end - start;
        decompress_times.push_back(diff.count());
    }
    
    auto decompress_stats = calculate_stats(decompress_times);
    result.decompress_min_ms = decompress_stats.min;
    result.decompress_max_ms = decompress_stats.max;
    result.decompress_avg_ms = decompress_stats.avg;
    result.decompress_std_ms = decompress_stats.std_dev;
    
    // 计算吞吐量
    double mb = static_cast<double>(result.original_size) / (1024.0 * 1024.0);
    if (result.compress_avg_ms > 0) {
        result.compress_throughput = mb / (result.compress_avg_ms / 1000.0);
    }
    if (result.decompress_avg_ms > 0) {
        result.decompress_throughput = mb / (result.decompress_avg_ms / 1000.0);
    }
    
    // 验证正确性
    result.verified = (decompressed == data);
    
    return result;
}

void print_results_table(const std::vector<DetailedBenchResult>& results) {
    std::cout << "\n";
    std::cout << std::left << std::setw(10) << "Algo"
              << std::setw(15) << "Test"
              << std::right
              << std::setw(10) << "Size(KB)"
              << std::setw(10) << "Comp(KB)"
              << std::setw(8) << "Ratio"
              << std::setw(10) << "C.Avg(ms)"
              << std::setw(10) << "D.Avg(ms)"
              << std::setw(10) << "C.MB/s"
              << std::setw(10) << "D.MB/s"
              << std::setw(6) << "OK"
              << "\n";
    
    std::cout << std::string(99, '-') << "\n";
    
    for (const auto& r : results) {
        std::cout << std::left << std::setw(10) << r.algorithm
                  << std::setw(15) << r.test_name
                  << std::right << std::fixed
                  << std::setw(10) << std::setprecision(1) 
                  << (r.original_size / 1024.0)
                  << std::setw(10) << std::setprecision(1) 
                  << (r.compressed_size / 1024.0)
                  << std::setw(8) << std::setprecision(3) << r.ratio
                  << std::setw(10) << std::setprecision(2) << r.compress_avg_ms
                  << std::setw(10) << std::setprecision(2) << r.decompress_avg_ms
                  << std::setw(10) << std::setprecision(1) << r.compress_throughput
                  << std::setw(10) << std::setprecision(1) << r.decompress_throughput
                  << std::setw(6) << (r.verified ? "Yes" : "NO!")
                  << "\n";
    }
}

void write_json_results(const std::string& path, 
                       const std::vector<DetailedBenchResult>& results) {
    std::ofstream out(path);
    out << "[\n";
    
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "  {\n"
            << "    \"algorithm\": \"" << r.algorithm << "\",\n"
            << "    \"test_name\": \"" << r.test_name << "\",\n"
            << "    \"original_size\": " << r.original_size << ",\n"
            << "    \"compressed_size\": " << r.compressed_size << ",\n"
            << "    \"ratio\": " << r.ratio << ",\n"
            << "    \"compress_avg_ms\": " << r.compress_avg_ms << ",\n"
            << "    \"compress_std_ms\": " << r.compress_std_ms << ",\n"
            << "    \"decompress_avg_ms\": " << r.decompress_avg_ms << ",\n"
            << "    \"decompress_std_ms\": " << r.decompress_std_ms << ",\n"
            << "    \"compress_throughput_mb_s\": " << r.compress_throughput << ",\n"
            << "    \"decompress_throughput_mb_s\": " << r.decompress_throughput << ",\n"
            << "    \"verified\": " << (r.verified ? "true" : "false") << "\n"
            << "  }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    
    out << "]\n";
}

void print_usage() {
    std::cout << "Usage: compressup_advanced_bench [OPTIONS]\n\n"
              << "Options:\n"
              << "  --size SIZE      Test data size in KB (default: 100)\n"
              << "  --runs N         Number of measurement runs (default: 10)\n"
              << "  --warmup N       Number of warmup runs (default: 2)\n"
              << "  --json PATH      Output results to JSON file\n"
              << "  --algo NAME      Test only specified algorithm\n"
              << "  --parallel       Include parallel compression tests\n"
              << "  --help           Show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    std::size_t data_size_kb = 100;
    int measurement_runs = 10;
    int warmup_runs = 2;
    std::string json_path;
    std::string single_algo;
    bool test_parallel = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "--size" && i + 1 < argc) {
            data_size_kb = std::stoul(argv[++i]);
        } else if (arg == "--runs" && i + 1 < argc) {
            measurement_runs = std::stoi(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            warmup_runs = std::stoi(argv[++i]);
        } else if (arg == "--json" && i + 1 < argc) {
            json_path = argv[++i];
        } else if (arg == "--algo" && i + 1 < argc) {
            single_algo = argv[++i];
        } else if (arg == "--parallel") {
            test_parallel = true;
        }
    }
    
    std::size_t data_size = data_size_kb * 1024;
    
    std::cout << "CompressUp Advanced Benchmark\n"
              << "=============================\n"
              << "Data size: " << data_size_kb << " KB\n"
              << "Warmup runs: " << warmup_runs << "\n"
              << "Measurement runs: " << measurement_runs << "\n"
              << "Threads available: " << std::thread::hardware_concurrency() << "\n\n";
    
    // 获取算法列表
    std::vector<std::string> algorithms;
    if (!single_algo.empty()) {
        algorithms.push_back(single_algo);
    } else {
        algorithms = available_algorithms();
    }
    
    // 测试数据类型
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"text", "模拟文本"},
        {"repetitive", "重复数据"},
        {"binary", "二进制数据"},
        {"sparse", "稀疏数据"},
        {"random", "随机数据"},
    };
    
    std::vector<DetailedBenchResult> all_results;
    
    // 运行所有测试
    for (const auto& [test_type, test_desc] : test_cases) {
        std::cout << "Generating " << test_desc << " (" << test_type << ")...\n";
        std::string test_data = generate_test_data(test_type, data_size);
        
        for (const auto& algo : algorithms) {
            std::cout << "  Testing " << algo << "...\r" << std::flush;
            
            try {
                auto result = run_detailed_bench(
                    algo, test_type, test_data, warmup_runs, measurement_runs);
                all_results.push_back(result);
            } catch (const std::exception& e) {
                std::cerr << "Error testing " << algo << " on " << test_type 
                          << ": " << e.what() << "\n";
            }
        }
    }
    
    // 并行压缩测试
    if (test_parallel && data_size >= 64 * 1024) {
        std::cout << "\nRunning parallel compression tests...\n";
        
        std::string test_data = generate_test_data("text", data_size);
        
        for (const auto& algo : algorithms) {
            std::cout << "  Testing parallel_" << algo << "...\r" << std::flush;
            
            try {
                auto base = create_compressor(algo);
                ParallelCompressor parallel(std::move(base), 16 * 1024);
                
                DetailedBenchResult result;
                result.algorithm = "parallel_" + algo;
                result.test_name = "text";
                result.original_size = test_data.size();
                
                // Warmup
                auto compressed = parallel.compress(test_data);
                result.compressed_size = compressed.size();
                result.ratio = static_cast<double>(result.compressed_size) / 
                               static_cast<double>(result.original_size);
                
                // 测量
                std::vector<double> compress_times;
                for (int i = 0; i < measurement_runs; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    auto tmp = parallel.compress(test_data);
                    auto end = std::chrono::steady_clock::now();
                    
                    std::chrono::duration<double, std::milli> diff = end - start;
                    compress_times.push_back(diff.count());
                }
                
                auto stats = calculate_stats(compress_times);
                result.compress_avg_ms = stats.avg;
                result.compress_std_ms = stats.std_dev;
                
                double mb = static_cast<double>(result.original_size) / (1024.0 * 1024.0);
                result.compress_throughput = mb / (result.compress_avg_ms / 1000.0);
                
                // 验证
                auto decompressed = parallel.decompress(compressed);
                result.verified = (decompressed == test_data);
                
                all_results.push_back(result);
            } catch (const std::exception& e) {
                std::cerr << "Error in parallel test for " << algo 
                          << ": " << e.what() << "\n";
            }
        }
    }
    
    // 输出结果
    print_results_table(all_results);
    
    // 输出JSON
    if (!json_path.empty()) {
        write_json_results(json_path, all_results);
        std::cout << "\nResults written to " << json_path << "\n";
    }
    
    // 统计验证失败
    int failures = 0;
    for (const auto& r : all_results) {
        if (!r.verified) ++failures;
    }
    
    if (failures > 0) {
        std::cerr << "\nWARNING: " << failures << " tests failed verification!\n";
        return 1;
    }
    
    std::cout << "\nAll tests passed.\n";
    return 0;
}
