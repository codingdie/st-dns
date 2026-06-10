# Forward 专用队列设计

## 背景

当上游 DNS 不可用时，`A` 记录查询会通过本地 100ms 定时器快速返回，但 `HTTPS(type 65)`、`SVCB`、`TXT`、`MX` 等非 `A/AAAA` 查询会走 `forward_dns_request()` 透传上游。现有实现直接使用上游 `timeout`，现代客户端大量发送 `HTTPS(type 65)` 查询时，会产生大量等待中的 forward 请求。

目标是用现有 `st::task::queue` 隔离 forward 压力：上游不可用时，forward 请求不能拖慢主 DNS 处理链路，也不能无限排队。

## 目标行为

- `A` 查询保持现有缓存与后台同步逻辑。
- `AAAA` 查询保持现有快速 drop 逻辑。
- 非 `A/AAAA` 查询进入独立 forward 队列。
- forward 并发未满时，允许请求透传到第一个 UDP 上游。
- forward 队列容量与并发上限相同，并发上限通过 `forward_max_running` 配置，默认 32。
- forward 队列满时，新请求立即拒绝，返回空 DNS 响应。
- 不做超出并发上限的排队等待；队列满就是拒绝。
- 上游请求完成或超时后释放并发槽位。

## 设计

在 `dns_server` 内增加 forward 专用 `st::task::queue<forward_task_queue_param>`。该队列参数参考现有同步队列，使用简单 `pair` 类型，不引入额外 payload struct。该队列设置 `max_running == max_size`，所以最多只保存并执行固定数量的 forward 任务；达到容量后，`submit()` 直接返回失败。

`st::task::queue` 增加可选 `max_size` 参数，默认值为 0 表示不限制。这样现有 record sync 队列保持原行为，forward 队列单独启用容量限制。

`forward_dns_request()` 流程：

1. 选择第一个 UDP 类型上游。
2. 如果没有可用 UDP 上游，直接返回空响应。
3. 将请求包装为 `forward_task_queue_param` 并提交到 forward 队列。
4. 如果提交失败，记录拒绝维度并立即调用 `complete_handler(session)`。
5. 如果提交成功，由队列任务调用 `dns_client::forward_udp()` 透传请求。
6. forward 回调触发后调用 `forward_task_queue.complete(task)` 释放队列槽位，并将上游响应写入 `session->response`。

默认并发上限为 32。这个值用于防止上游不可用时的请求放大，同时允许正常短突发通过；可在 `config.json` 中用 `forward_max_running` 覆盖。

## 错误处理

- 队列满：立即空响应，不等待上游。
- 没有 UDP 上游：立即空响应。
- 上游超时或错误：沿用 `dns_client::forward_udp()` 当前行为，回调返回空响应并释放槽位。
- 回调必须只完成一次，避免超时和网络回调竞态导致重复释放或重复响应。

## 测试

新增集成测试覆盖并发满拒绝：

- 将 forward 并发上限设为较小值。
- 配置不可用 UDP 上游。
- 并发发起多条 `HTTPS(type 65)` 查询。
- 断言超过并发上限的请求能快速收到空响应。
- 断言总耗时不随上游 timeout 线性增长。

保留单请求场景，确认并发未满时 forward 路径仍会进入上游透传，并按上游 timeout 完成。

## 非目标

- 不实现上游健康检查。
- 不拆分 forward 队列容量和并发上限配置。
- 不改变 `A` 查询缓存逻辑。
- 不改变 `dns_client` 的 UDP/TCP/DoT 实现。
