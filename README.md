# miso-server

这个仓库是游戏 **MiSO** (GGCC Minesweeper Online) 的服务器部分。原仓库因多种问题无法维护，因此我们决定重写游戏逻辑，并开源所有代码，希望更多人能参与**MiSO**的开发。

## 环境

本项目使用 CMake 构建，使用 vcpkg 作为包管理器，通过 vcpkg 的清单模式管理第三方库，因此请确保你已经安装以下工具。

- CMake (>=3.10)
- vcpkg
- gcc (>=11.2)

## 构建

克隆仓库

```
git clone https://github.com/wangxun2008/miso_server.git
```

Linux 下构建
```
cd miso_server
cmake --preset linux-debug
cmake --build --preset linux-debug
```

Windows 下构建
```
cd miso_server
cmake --preset mingw-debug
cmake --build --preset mingw-debug
```
