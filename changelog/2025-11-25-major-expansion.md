# 2025-11-25: 项目架构大规模扩展

## 概述

本次更新对CompressUp项目进行了大规模扩展，新增多种压缩/编码算法、多线程支持、高级IO系统和增强的基准测试框架。

## 新增压缩算法

### 熵编码类
- **Huffman编码** (`huffman_compressor.{h,cpp}`)
  - 基于字符频率构建最优二叉树
  - 高频字符使用短编码，低频字符使用长编码
  - 适用于一般文本压缩

### 字典压缩类
- **LZW算法** (`lzw_compressor.{h,cpp}`)
  - Lempel-Ziv-Welch字典压缩
  - 动态构建字典，无需预先传输字典
  - 12位编码，最大字典大小4096

- **LZSS算法** (`lzss_compressor.{h,cpp}`)
  - LZ77的优化变体
  - 使用标志位区分字面量和匹配引用
  - 滑动窗口4096字节，最小匹配长度3

### 变换类
- **Delta编码** (`delta_compressor.{h,cpp}`)
  - 存储相邻字节差值
  - 适用于有平滑变化的数据

- **BWT+MTF** (`bwt_compressor.{h,cpp}`)
  - Burrows-Wheeler变换 + Move-to-Front编码
  - 将相似字符聚集，利用局部性
  - 块大小限制100KB

## 多线程框架

新增 `parallel_compressor.{h,cpp}`:

- **ThreadPool**: 通用线程池实现
  - 自动检测硬件并发数
  - 支持任务提交和异步结果获取

- **ParallelCompressor**: 并行压缩包装器
  - 将输入分块并行处理
  - 支持进度回调
  - 可配置块大小和线程数

## 高级IO系统

新增 `advanced_io.{h,cpp}`:

- **MappedFile**: 内存映射文件读取
  - 使用mmap实现零拷贝读取
  - 自动管理资源（RAII）

- **BufferedWriter**: 缓冲写入器
  - 减少系统调用次数
  - 可配置缓冲区大小

- **StreamReader**: 流式读取器
  - 支持大文件分块读取
  - 自动缓冲管理

- **AsyncIO**: 异步IO操作
  - 异步文件读写
  - 异步压缩/解压

## 增强的类型系统

更新 `types.h`:

- **CompressionLevel**: 压缩级别枚举
- **AlgorithmCategory**: 算法类别（熵编码/字典/变换/混合）
- **CompressionStats**: 压缩统计信息
- **ProgressCallback**: 进度回调函数类型

## 增强的注册系统

更新 `registry.{h,cpp}`:

- 新增 `AlgorithmInfo` 结构，包含算法名称、描述和类别
- 新增 `available_algorithm_infos()` 返回详细算法信息
- 新增 `algorithms_by_category()` 按类别查询算法

## 高级基准测试

新增 `bench/advanced_bench.cpp`:

- 多种测试数据类型：文本、重复、二进制、稀疏、随机
- 详细统计：最小/最大/平均/标准差
- JSON输出支持
- 并行压缩性能测试
- 自动验证压缩正确性

## CMake更新

- 添加所有新源文件
- 添加pthread链接
- 添加编译警告选项
- 添加可选LTO支持
- 添加安装规则

## 测试增强

- 新增长字符串测试
- 新增并行压缩测试
- 新增算法信息查询测试
- 改进测试输出格式

## 新增文件列表

```
src/
├── huffman_compressor.h
├── huffman_compressor.cpp
├── lzw_compressor.h
├── lzw_compressor.cpp
├── lzss_compressor.h
├── lzss_compressor.cpp
├── delta_compressor.h
├── delta_compressor.cpp
├── bwt_compressor.h
├── bwt_compressor.cpp
├── parallel_compressor.h
├── parallel_compressor.cpp
├── advanced_io.h
└── advanced_io.cpp

bench/
└── advanced_bench.cpp
```

## 扩展新算法的步骤

1. 在 `src/` 创建 `xxx_compressor.{h,cpp}` 实现 `ICompressor` 接口
2. 在 `AlgorithmId` 枚举添加新ID
3. 在 `registry.cpp` 的 `kAlgorithmInfos` 添加算法信息
4. 在 `create_compressor()` 函数添加创建逻辑
5. 在 `algorithm_id_from_name()` 和 `algorithm_name_from_id()` 添加映射
6. 在 `CMakeLists.txt` 添加源文件
7. 重新编译，测试和benchmark会自动包含新算法

## Git 配置

- 新增 `.gitignore` 文件，忽略 `build/`、`CMakeFiles/`、`CMakeCache.txt`、`Makefile`、`*.o` 等构建与编辑器临时文件。
