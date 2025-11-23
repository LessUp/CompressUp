#include "api.h"
#include "registry.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cout << "Usage:\n"
              << "  compressup_cli compress --algo <name> <input> <output>\n"
              << "  compressup_cli decompress <input> <output>\n"
              << "  compressup_cli list-algorithms\n";
}

} // namespace

int main(int argc, char** argv) {
    using namespace compressup;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "compress") {
            if (argc != 6) {
                print_usage();
                return 1;
            }

            std::string algo_flag = argv[2];
            if (algo_flag != "--algo") {
                print_usage();
                return 1;
            }

            std::string algorithm_name = argv[3];
            std::string input_path = argv[4];
            std::string output_path = argv[5];

            compress_file(input_path, output_path, algorithm_name);
            return 0;
        } else if (command == "decompress") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            std::string input_path = argv[2];
            std::string output_path = argv[3];

            decompress_file(input_path, output_path);
            return 0;
        } else if (command == "list-algorithms") {
            auto algos = available_algorithms();
            for (const auto& a : algos) {
                std::cout << a << "\n";
            }
            return 0;
        } else {
            print_usage();
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
