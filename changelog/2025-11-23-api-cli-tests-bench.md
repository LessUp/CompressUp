# 2025-11-23 API / CLI / Tests / Bench

- 新增高层 API：`api.h/.cpp`，提供 `compress_file` / `decompress_file` 封装文件读写与容器格式。
- 新增命令行工具入口：`src/main.cpp`，支持命令：
  - `compressup_cli compress --algo <name> <input> <output>`
  - `compressup_cli decompress <input> <output>`
  - `compressup_cli list-algorithms`
- 新增基础正确性测试：`tests/test_main.cpp`，对 `rle` 和 `lz77` 算法做多种字符串往返测试。
- 新增基准测试程序：`bench/bench_main.cpp`，对每个算法和输入文件输出压缩率、压缩/解压耗时与吞吐量，方便对比性能。
