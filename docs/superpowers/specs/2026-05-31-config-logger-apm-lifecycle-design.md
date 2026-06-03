# Config / Logger / APM / AreaIP 生命周期重构设计

日期：2026-05-31

## 背景

当前项目存在一组相互耦合的运行时生命周期问题：

- `config::load()` 会触发 `logger::init()`。
- `logger::init()` 当前会隐式启动 `apm_logger::init()`，因此“初始化日志”等价于“启动后台线程池与定时器”。
- `areaip::manager` 采用“单例构造即启动”的模式，线程、timer、io_context 生命周期不受 `config` 显式编排。
- 测试 fixture 中反复 `config::load()` 后，如果没有完整、对称的卸载顺序，会出现：
  - GoogleTest 已经通过但进程不退出，最终被 CTest 超时杀死；
  - 退出阶段后台线程仍尝试写日志，触发崩溃或 SegFault；
  - 重复 load/unload 过程中出现隐藏副作用，难以推导资源所有权。

本次重构目标不是“临时修掉某个超时测试”，而是建立一套明确、可重复、可测试的运行时生命周期模型。

## 目标

1. 由 `config` 统一管理运行时子系统生命周期。
2. 解耦普通 `logger` 与 `apm_logger` 的职责。
3. 保持 APM 默认启用，且不提供关闭配置。
4. 让 `areaip::manager` 从“构造即启动”改为“显式 start/stop”。
5. 保证 `load/unload`、`start/stop`、`init/disable` 都是幂等的。
6. 支持测试中每个用例独立 `load -> run -> unload`，进程稳定退出。

## 非目标

1. 不引入新的总控对象（如 `runtime_context` / `service_runtime`）。
2. 不修改 APM 统计语义、采样逻辑、落盘格式。
3. 不调整 AreaIP 业务逻辑与下载逻辑，只处理其生命周期管理。
4. 不在本次重构中解决所有历史 warning。

## 总体方案

采用“`config` 作为生命周期编排器，子系统自行提供显式生命周期接口”的方案：

- `config`
  - 负责读取配置与编排启动/关闭顺序。
- `logger`
  - 只负责普通日志 sink 管理。
- `apm_logger`
  - 只负责异步统计线程池与定时汇报。
- `areaip::manager`
  - 只负责 AreaIP 后台任务与缓存同步线程。

设计原则：

- 所有后台资源都必须有明确 owner。
- 所有启动 API 都必须有对称关闭 API。
- 停机顺序必须固定且显式。
- 重复调用不应 double free、重复起线程或造成悬空回调。

## 组件职责与接口

### 1. config

保留并强化以下接口：

- `void load(const string& base_conf_dir)`
- `void unload()`
- `bool loaded` 状态位（或等价访问方式）

语义：

- `load()`
  - 若已加载，先执行 `unload()`。
  - 读取配置文件。
  - 初始化普通 logger。
  - 显式启动 APM。
  - 配置并启动 `areaip::manager`。
  - 加载 `servers` 与 `force_resolve_rules`。
  - 标记 `loaded = true`。
- `unload()`
  - 若未加载，直接返回。
  - 先停 `areaip::manager`。
  - 再停 `apm_logger`。
  - 再停 `logger`。
  - 清理 `servers` 与 `force_resolve_rules`。
  - 恢复默认配置字段。
  - 标记 `loaded = false`。

推荐停机顺序：

1. `areaip::manager::stop()`
2. `apm_logger::disable()`
3. `logger::disable()`
4. 纯数据清理

原因：

- `areaip` 后台任务可能继续记录普通日志和 APM 数据。
- 如果先停 logger/APM，后停 areaip，则 areaip 的异步回调可能在退出阶段访问已关闭的日志基础设施。

### 2. logger

保留接口：

- `static void init(const boost::property_tree::ptree& tree)`
- `static void disable()`
- `static bool INITED`（或等价状态）

重构后的语义：

- `logger::init()` 只负责：
  - 读取日志 level/tag
  - 配置 Boost.Log sink
  - 设置 `INITED = true`
- `logger::disable()` 只负责：
  - flush / remove sinks
  - reset filter
  - 设置 `INITED = false`

关键约束：

- `logger::init()` 不再隐式调用 `apm_logger::init()`。
- `logger::disable()` 不再顺带关闭 `apm_logger`。

结果：

- “日志初始化”不再意味着“启动后台线程池”。
- `logger` 生命周期可以单独理解和验证。

### 3. apm_logger

保留接口并强化语义：

- `static void init()`
- `static void disable(bool report_status_log = true)`
- 增加显式状态位，例如：`STARTED`

重构后的语义：

- `init()`
  - 若已启动，直接返回。
  - `IO_CONTEXT.restart()`。
  - 创建 `work`。
  - 拉起后台线程池。
  - 注册周期 timer。
  - 标记 `STARTED = true`。
- `disable()`
  - 若未启动，直接返回。
  - cancel timer。
  - 释放 `work`。
  - stop `io_context`。
  - join 所有线程。
  - flush 剩余统计。
  - 清理状态。
  - 标记 `STARTED = false`。

约束：

- APM 默认始终开启，不提供配置开关。
- 但其生命周期不再绑死在 `logger::init()` 内部，而是由 `config` 显式启动。

### 4. areaip::manager

新增/强化接口：

- `void start()`
- `void stop()`
- `bool started() const`（或等价状态）
- 现有 `config(const area_ip_config&)` 保持只设置配置

重构后的语义：

- 构造函数
  - 只初始化轻量级内存状态：默认 LAN 段、本地容器、随机引擎等
  - 不启动线程
  - 不创建 timer/work
- `start()`
  - 幂等
  - 创建 `ctx_work` / `sche_ctx_work`
  - 启动 `th` / `sche_th`
  - 创建 `sync_timer`
  - 启动 `sync_net_area_ip()`
- `stop()`
  - 幂等
  - cancel timer
  - release work
  - stop io_context
  - join threads
  - 清空指针和 started 状态

关键变化：

- 移除“单例第一次构造就永久起线程”的模式。
- `config` 成为其唯一高层生命周期 owner。

## 启停顺序设计

### 启动顺序

`config::load()` 内部顺序：

1. `unload()`（如当前已加载）
2. 读取 JSON
3. `logger::init(tree)`
4. `apm_logger::init()`
5. `areaip::manager::uniq().config(area_ip_config)`
6. `areaip::manager::uniq().start()`
7. 加载服务器与规则
8. `loaded = true`

原因：

- `areaip` 启动后可能立即产生日志，因此 logger 必须先可用。
- `areaip` 启动后可能记录 APM，因此 APM 必须先可用。

### 关闭顺序

`config::unload()` 内部顺序：

1. `areaip::manager::uniq().stop()`
2. `apm_logger::disable()`
3. `logger::disable()`
4. 清理 servers / rules / 基础字段
5. `loaded = false`

原因：

- 先停业务后台源头，再停统计和日志基础设施。

## 状态机与幂等性要求

每个组件都必须维护明确状态，不允许依赖“指针是否为空”作为唯一事实来源。

推荐状态：

- `config.loaded`
- `logger.INITED`
- `apm_logger.STARTED`
- `areaip::manager.started_`

幂等规则：

- 重复 `load()`：等价于 `unload(); load();`
- 重复 `unload()`：无副作用
- 重复 `logger::init()`：安全；必要时先内部清理旧 sink 或直接短路
- 重复 `logger::disable()`：安全
- 重复 `apm_logger::init()` / `disable()`：安全
- 重复 `areaip::manager::start()` / `stop()`：安全

建议在生命周期入口加互斥保护，避免未来并发触发时状态撕裂。

## 对现有代码语义的影响

本次重构会改变一条旧语义：

- 旧语义：`logger::init()` 会顺带启动 APM。
- 新语义：`logger::init()` 只初始化普通日志；APM 由 `config::load()` 显式启动。

这是预期改动，不是兼容性事故。其价值在于：

- 普通日志与后台异步统计解耦
- 生命周期边界更直观
- 测试更容易构造最小环境

## 测试策略

### 1. 生命周期回归测试

新增或补强以下场景：

- `config::INSTANCE.load("../confs/test")`
- `config::INSTANCE.unload()`
- 重复执行两次以上，不崩溃、不 hang

### 2. 超时回归测试

重点验证此前存在退出问题的测试：

- `cache_management.*`
- `reverse_resolve.*`
- `cache_integration_tests.*`

预期：

- GoogleTest 通过后，测试进程能够正常退出
- 不再出现 CTest 120 秒超时

### 3. 退出阶段崩溃回归测试

重点验证：

- 反复 load/unload 后，不再出现退出阶段 SegFault
- AreaIP 后台线程停止后，不再向已关闭 logger/APM 写数据

### 4. 全量验证

必须运行：

```bash
cd build && ctest --output-on-failure -j1
```

## 实施步骤

1. 拆分 `logger::init()` 与 `apm_logger::init()` 的隐式耦合
2. 给 `apm_logger` 增加显式 started 状态与完整幂等关闭逻辑
3. 给 `areaip::manager` 增加 `start/stop`，移除构造即启动
4. 让 `config::load/unload` 成为唯一高层编排入口
5. 更新单测/集成测试 fixture，统一通过 `config::load/unload` 管理生命周期
6. 先跑定向回归测试，再跑全量串行 CTest

## 风险与注意事项

1. `areaip::manager` 当前是单例，停掉后再启动必须保证内部 `io_context`、timer、work、线程指针全部可重复使用。
2. `apm_logger` 停机后需要确保下次 `init()` 能正确 `restart()`，并且不会保留旧线程句柄。
3. `logger::disable()` 之后若仍有后台线程调用日志接口，说明关闭顺序有问题，这应视为设计缺陷而不是测试偶发。
4. 现有某些测试可能依赖“logger 初始化自动带起 apm”的旧行为，需要调整为通过 `config::load()` 或显式 `apm_logger::init()` 建立运行态。

## 验收标准

满足以下条件视为设计完成：

1. `config::load/unload` 成为运行时生命周期唯一高层入口。
2. `logger` 与 `apm_logger` 职责分离。
3. `areaip::manager` 不再构造即启动。
4. 相关测试不再因进程不退出而超时。
5. 退出阶段不再出现由后台线程写日志/写 APM 引发的 SegFault。
6. 全量 `ctest --output-on-failure -j1` 通过。
