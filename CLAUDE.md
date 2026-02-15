# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 额外说明
这个项目作为openwrt package提供出去，需要同时修改插件代码，插件代码在../home-openwrt/codingdie-packages/packages/st-dns下，你需要阅读和修改此代码

## Project Overview

st-dns is a smart local DNS server that supports configurable multi-server DNS chains (UDP/TCP/DoT). It intelligently resolves IPs based on geographic area restrictions, with caching and area-based optimization capabilities.

## Build System

This project uses CMake with protobuf code generation. The build system has specific requirements:

### Building

```bash
# Development build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Install (default: /usr/local)
./install.sh

# Install to custom prefix
./install.sh /custom/path
```

### Running Tests

```bash
# Build and run all tests (tests only build when OPENWRT=OFF)
cd build
ctest

# Run specific test executables
./st-unit-test           # Common utilities tests
./st-dns-unit-test       # DNS core unit tests
./st-dns-integration-test # Integration tests

# Note: Tests are disabled in OpenWrt builds
```

### Protobuf Code Generation

The project uses protobuf for serialization. Generated files are placed in `${CMAKE_CURRENT_BINARY_DIR}`:
- `src/core/protocol/message.proto` → `message.pb.{h,cc}`
- `src/common/main/proto/kv.proto` → `kv.pb.{h,cc}`

**Important**: The build directory must be in the include path for generated headers to be found. This is already configured in CMakeLists.txt via `include_directories(${CMAKE_CURRENT_BINARY_DIR})`.

**Common Build Issue**: If you see `fatal error: kv.pb.h: No such file or directory`, ensure:
1. The build directory is in the include path
2. Generated protobuf sources are added to the target: `${MESSAGE_PROTO_SRCS} ${KV_PROTO_SRCS}`

## Architecture

### Core Components

**dns_server** (`src/core/dns_server.{h,cpp}`)
- Main server class that binds to local UDP socket
- Receives DNS queries from clients
- Manages session lifecycle and query processing
- Coordinates between caching, forwarding, and remote resolution

**session** (`src/core/session.{h,cpp}`)
- Represents a single DNS query from a client
- Contains request, response, and processing metadata
- Tracks whether query should be answered from cache (QUERY) or forwarded (FORWARD)

**dns_client** (`src/core/dns_client.{h,cpp}`)
- Handles outbound DNS queries to upstream servers
- Supports UDP, TCP, and TCP+TLS (DoT) protocols
- Manages timeouts and connection pooling

**dns_record_manager** (`src/core/dns_record_manager.{h,cpp}`)
- Persistent DNS cache using LevelDB
- Stores DNS records with expiration times
- Supports reverse IP lookups
- Handles area-based IP filtering and prioritization

**config** (`src/core/config.{h,cpp}`)
- Loads configuration from JSON files
- Defines remote_dns_server chain with area restrictions
- Configuration paths: `/etc/st/dns`, `/usr/local/etc/st/dns`, or `../confs`

### DNS Query Flow

1. Client sends UDP DNS query → dns_server receives on port 53
2. dns_server creates session and checks cache via dns_record_manager
3. If cached and not expired → return cached response
4. If not cached → select appropriate upstream servers based on domain/area
5. dns_client queries upstream servers (UDP/TCP/DoT)
6. Response cached in dns_record_manager with TTL
7. Response sent back to client

### Server Selection Logic

The `remote_dns_server::select_servers()` method chooses upstream servers based on:
- Domain whitelist/blacklist per server
- Geographic area restrictions (CN, US, JP, etc.)
- Server order in configuration chain

### Directory Structure

```
src/
├── core/              # DNS server core logic
│   ├── protocol/      # DNS protocol parsing (message.proto)
│   ├── dns_server.*   # Main server
│   ├── dns_client.*   # Upstream DNS client
│   ├── session.*      # Query session
│   ├── dns_record_manager.* # Cache management
│   └── config.*       # Configuration
├── common/main/       # Shared utilities
│   ├── kv/           # Key-value storage (LevelDB wrapper)
│   ├── proto/        # Protobuf definitions (kv.proto)
│   ├── utils/        # File, network, string utilities
│   ├── taskquque/    # Task queue implementation
│   └── thirdpart/    # Third-party headers (httplib.h)
├── server/           # Entry point (main.cpp)
└── test/             # Tests
    ├── unit/         # Unit tests
    └── integration/  # Integration tests
```

## Configuration

Configuration is JSON-based. See `confs/linux/config-example.json` for reference.

Key configuration elements:
- `ip`/`port`: Local bind address (default: 127.0.0.1:53)
- `servers`: Array of upstream DNS servers with type (UDP/TCP/TCP_SSL), areas, timeouts
- `dns_cache_expire`: Global cache TTL
- `area_resolve_optimize`: Enable area-based IP optimization (requires st-proxy)

Configuration search paths (in order):
1. Path specified via `-c` flag
2. `../confs` (relative to binary)
3. `/usr/local/etc/st/dns`
4. `/etc/st/dns`

## Dependencies

Required libraries (see README.md for versions):
- CMake >= 3.7.2
- Boost >= 1.66.0 (system, filesystem, thread, program_options, log, log_setup)
- OpenSSL >= 1.1.0
- Curl
- LevelDB
- Protobuf

## Running the Server

```bash
# Direct run (searches default config paths)
st-dns

# Specify config directory
st-dns -c /path/to/config

# Run as daemon
sudo st-dns -d start
sudo st-dns -d stop
sudo st-dns -d restart
```

## OpenWrt Build

This project is designed to build for OpenWrt. The CMakeLists.txt includes special handling when `OPENWRT=ON`:
- Skips Google Test dependencies
- Installs OpenWrt-specific configs from `confs/openwrt/`
- Uses OpenWrt toolchain and staging directories

The project is packaged in the codingdie-packages feed for OpenWrt. Build errors in OpenWrt typically relate to:
- Missing protobuf include paths
- Cross-compilation toolchain issues
- Dependency version mismatches

## Key Implementation Details

### Protobuf Usage
- `message.proto`: DNS record serialization for network protocol
- `kv.proto`: Value wrapper with expiration for LevelDB storage

### Caching Strategy
- Two-level caching: in-memory + LevelDB persistent cache
- Records stored with expiration timestamps
- Reverse lookup index (IP → domains) maintained

### Logging
- Custom logging via `st.h` logger macros
- Optional UDP log servers for raw logs and APM metrics
- Log levels: DEBUG/INFO/WARN/ERROR

### Thread Model
- Main thread: UDP socket receive loop
- dns_client: Separate io_context thread for upstream queries
- dns_record_manager: Separate thread for background tasks
- Task queue for async remote record synchronization

### Common Utilities (src/common/main/)
- **kv/**: Key-value storage abstraction with disk_kv (LevelDB) and abstract_kv interface
- **utils/**: File operations, network utilities, string helpers
- **taskquque/**: Task queue for async operations
- **console/**: Console management for runtime control
- **command/**: Command processing framework

### Build System Notes
- Uses `file(GLOB_RECURSE)` to collect source files automatically
- Protobuf files must be explicitly added to targets
- Third-party header `httplib.h` is auto-downloaded during CMake configuration if missing

## Claude 工作习惯

### 语言偏好

默认使用中文进行交流和编写代码注释。

### Git 配置与规范
**重要：所有 Git 操作必须遵循以下规范**

- 用户名：codingdie
- 邮箱：codingdie@gmail.com
- 所有提交必须使用此身份
- **不要在 commit message 中添加 Co-Authored-By 标签**
- 修改代码后**不要自动 commit**
- 等待用户明确说"提交"、"commit"或"push"后，再执行 `git commit` + `git push`
- 可以使用 `git diff` 或 `git status` 查看改动，但不要自动提交

### 开发流程规范

**重要：每次开始功能开发前，必须阅读并遵循 [DEVELOPMENT.md](DEVELOPMENT.md) 中的开发流程规范**

DEVELOPMENT.md 包含完整的开发流程、代码规范、测试规范和 Git 工作流：

1. **功能开发流程**: 从创建功能文档到测试部署的完整流程
2. **功能文档规范**: 文档命名规则 `F{编号}-{年}-{月}-{日}.md`、文档结构要求
3. **代码开发规范**: 代码风格、模块化、错误处理、性能和安全考虑
4. **测试规范**: 单元测试、集成测试、测试命名和覆盖要求
5. **Git 工作流**: 分支管理、提交规范、禁止操作
6. **文档更新流程**: 功能文档、架构文档、README 的更新时机
7. **Claude 开发注意事项**: 开发前、开发中、开发后的检查清单

**工作流程**:
- 用户创建功能文档 → 更新功能索引 → 通知 Claude 开发
- Claude 阅读功能文档 → 理解需求和技术方案 → 自动编写代码
- Claude 编写测试用例 → 运行测试验证 → 更新文档状态
