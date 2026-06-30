#!/bin/bash

# 自动化 QT 编译脚本

echo ">>> 正在清理旧构建..."
rm -rf build

echo ">>> 创建新构建目录..."
mkdir -p build

echo ">>> 进入构建目录并配置 CMake..."
cd build

cmake .. 

if [ $? -ne 0 ]; then
    echo "!!! CMake 配置失败，请检查错误信息。"
    exit 1
fi

echo ">>> 开始编译 (使用 $(nproc) 个线程)..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ">>> 编译成功！可执行文件位于: $(pwd)/meChatServer "
    echo ">>> 运行服务meChatServer..."
    ./meChatServer
else
    echo "!!! 编译失败。"
    exit 1
fi