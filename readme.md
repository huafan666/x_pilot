# X-Pilot 智能农业机器人中控系统

## 项目背景

本项目为海南农科院智慧农业项目配套开发，针对农业喷洒无人机/无人车在田间作业中的**系统稳定性不足、多进程协同效率低、4G/5G 网络通信可靠性差**等痛点，设计实现了一套高可靠、实时性强的嵌入式 Linux 中控系统，作为农业机器人的"神经中枢"，负责协调通信、控制、状态管理等核心任务。

## 系统架构

采用**多进程架构**，Manager 守护进程统一管理通信进程和控制进程，通过 Unix Domain Socket + 共享内存实现高效 IPC，通过 TCP + 心跳机制实现与地面站的可靠通信。

```
                    ┌─────────────────────────────────┐
                    │         X-Pilot 中控系统          │
                    │                                 │
                    │  ┌───────────────────────────┐  │
                    │  │  Manager 守护进程           │  │
                    │  │  进程监控 / 崩溃自愈 / 优雅退出│  │
                    │  └──┬──────────┬──────────┬───┘  │
                    │     │fork      │fork      │fork  │
                    │     ▼          ▼          ▼      │
                    │  ┌──────┐  ┌────────┐  ┌──────┐  │
                    │  │ Comm │◄─┤Control │  │ View │  │
                    │  │      │  │        │  │      │  │
                    │  │TCP客户端│ │共享内存写│  │只读  │  │
                    │  │心跳重连│  │指令处理 │  │可视化│  │
                    │  └──┬───┘  └───┬────┘  └──┬───┘  │
                    │     │IPC       │shm       │shm    │
                    └─────┼──────────┼──────────┼──────┘
                          │TCP       │          │
                          ▼          ▼          ▼
                   ┌────────────┐ ┌────────┐ ┌────────┐
                   │ 地面站服务器 │ │/dev/shm│ │ 终端输出│
                   │ (TCP Server)│ │共享内存 │ │        │
                   └────────────┘ └────────┘ └────────┘
```

## 技术栈

- **语言/标准**：C++14
- **构建工具**：CMake 3.10+
- **平台**：嵌入式 Linux（WSL/Ubuntu 开发环境）
- **第三方库**：nlohmann/json（JSON 解析）、Google Test（单元测试）
- **版本控制**：Git，遵循 GitFlow 分支管理流程

## 核心功能与实现

### 1. Manager 守护进程

- **进程生命周期管理**：`fork`+`exec` 拉起 Comm/Control/View 子进程，通过 `waitpid(WNOHANG)` 非阻塞监控
- **崩溃自愈**：子进程异常退出后延迟 3 秒自动重启；10 秒内重启超过 3 次则停止重启并报警
- **优雅退出**：捕获 `SIGTERM`/`SIGINT`，向所有子进程发送 `SIGTERM`，`waitpid` 循环回收，超时 5 秒后 `SIGKILL` 兜底，无僵尸进程

### 2. Comm 通信进程

- **TCP 客户端**：非阻塞 `connect` + `epoll` 事件驱动，指数退避重连（1s→2s→4s→…→60s）
- **心跳机制**：每 1 秒发送 `{"type":"heartbeat"}` 心跳包，5 秒未收到服务器响应判定断线并触发重连
- **协议封装/解析**：`4 字节网络字节序长度头 + JSON body`，基于接收缓冲区的状态机拆包，正确处理半包/粘包
- **数据转发**：epoll 同时监听 TCP 和 IPC 两个 fd，双向转发地面站↔Control 数据

### 3. Control 控制进程

- **状态管理**：工作线程每 100ms 更新机器人状态（位置 `x += v*0.1`、喷洒量累计、电量递减），写入共享内存
- **指令处理**：通过 Unix Domain Socket 接收 Comm 转发的指令（`SET_SPEED`/`START_SPRAY`/`STOP_SPRAY` 等），解析后更新状态
- **共享内存**：`shm_open`+`mmap` 创建共享内存区域，供外部调试工具（View 进程）零拷贝读取实时状态

### 4. 公共模块

- **日志系统**：统一格式 `[时间][级别][文件:行号] 消息`，支持 DEBUG/INFO/WARN/ERROR 四级，按天轮转
- **配置加载**：从 `config.json` 读取服务器 IP/端口、进程路径、心跳参数等，缺字段自动用默认值并告警
- **RAII 资源管理**：`ScopedFd` 封装文件描述符、`ScopedShm` 封装共享内存，析构自动释放，防止资源泄漏
- **数据埋点**：6 类结构化事件（`process_start`/`process_crash`/`tcp_connect_success`/`tcp_connect_fail`/`command_received`/`heartbeat_timeout`）单独写入 metrics.log

## 通信协议

### TCP 协议（Comm ↔ 地面站）

```
| 4 字节长度 (网络字节序) | N 字节 JSON 数据 |
```

```json
// 心跳包
{"type": "heartbeat", "timestamp": 1691234567}

// 状态上报
{"type": "status_update", "data": {"x": 123.45, "y": 678.90, "speed": 1.5, "battery": 85}}

// 控制指令
{"type": "command", "cmd": "SET_SPEED", "params": {"speed": 2.0}}
```

### Unix Domain Socket 协议（Comm ↔ Control）

帧格式同 TCP，使用本机字节序，无需网络序转换。

## 目录结构

```
x_pilot/
├── CMakeLists.txt          # 构建脚本（含 gtest 集成）
├── config/
│   ├── config.json         # 系统配置（IP/端口/路径/心跳参数）
│   └── config_plane.json   # 飞行任务参数
├── include/
│   ├── types.h             # 机器人状态结构体 + 指令枚举
│   ├── utils.h             # 日志接口 + LOG_* 宏
│   ├── config.h            # 配置加载器接口
│   ├── ipc.h               # Unix Domain Socket 封装
│   ├── tcp_client.h        # TCP 客户端封装
│   ├── protocol.h          # 协议拆包函数
│   ├── raii.h              # RAII 资源封装（ScopedFd/ScopedShm）
│   └── metrics.h           # 数据埋点接口
├── src/
│   ├── manager_main.cpp    # 守护进程：进程监控/重启/优雅退出
│   ├── comm_main.cpp       # 通信进程：epoll 事件循环/TCP↔IPC 转发
│   ├── control_main.cpp    # 控制进程：状态仿真/指令处理/共享内存
│   ├── view_main.cpp       # 可视进程：只读共享内存
│   ├── tcp_client.cpp      # TCP 客户端：非阻塞连接/心跳/重连
│   ├── ipc.cpp             # IPC 收发实现
│   ├── protocol.cpp        # 粘包/半包拆包实现
│   ├── config.cpp          # JSON 配置解析实现
│   ├── utils.cpp           # 日志实现（按天轮转）
│   └── metrics.cpp         # 埋点实现
└── test/
    ├── test_logger.cpp      # 日志模块单元测试
    ├── test_config.cpp      # 配置加载单元测试
    ├── test_protocol.cpp    # 协议拆包单元测试（粘包/半包/坏包）
    ├── ipc_test_client.cpp  # IPC 手动测试客户端
    └── tpc_test/            # TCP 协议 Python 测试脚本
```

## 构建与运行

```bash
# 构建
cd build
cmake ..
cmake --build .

# 运行单元测试
ctest --output-on-failure

# 启动系统
./manager    # 自动拉起 comm_process / control_process / view_process
```

## 个人职责

- 独立完成需求分析、系统架构设计、通信协议制定
- 实现多进程架构（Manager/Comm/Control/View）及全部核心功能
- 编写 CMake 构建脚本、单元测试、集成测试脚本
- 撰写 PRD 产品需求文档及设计文档

## 技术亮点

- **多进程通信**：Unix Domain Socket + 共享内存双通道，IPC 延迟 ≤5ms，共享内存更新频率 100ms
- **网络可靠性**：非阻塞 connect + epoll 事件驱动 + 指数退避重连 + 心跳超时检测，实现网络抖动下的自动恢复
- **协议健壮性**：基于接收缓冲区的状态机拆包，正确处理 TCP 流式传输中的半包/粘包问题
- **资源安全**：RAII 封装 fd/共享内存，信号处理函数遵循 async-signal-safe 规范，确保任意退出路径无资源泄漏
- **可观测性**：结构化数据埋点覆盖进程生命周期、网络连接、指令处理 6 类关键事件
- **工程化**：Google Test 单元测试覆盖核心模块，CMake 统一构建管理，Git 版本控制
