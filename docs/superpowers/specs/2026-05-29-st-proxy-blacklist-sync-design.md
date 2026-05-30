# ST Proxy IP 黑名单同步设计

**日期**: 2026-05-29
**状态**: 待用户 review

## 背景

ST Proxy 会基于连接质量生成 IP 黑名单。st-dns 当前仍可能从 DNS 缓存中返回这些 IP，导致客户端继续解析到 ST Proxy 已判定不可用的目标 IP。

目标是在 st-dns 中定时向 ST Proxy 获取黑名单，并让黑名单 IP 不再参与 DNS A 记录返回。

## 已确认需求

- 使用 ST Proxy 现有 console 命令 `proxy blacklist` 获取黑名单。
- 同步逻辑放在 st-dns 现有 `dns_server::schedule()` 中，复用当前 5 秒周期。
- 对黑名单 IP 同时做两层处理：
  - 物理清理 DNS cache 中受影响的 IP。
  - 解析返回前兜底过滤内存黑名单。
- 清理不能每 5 秒遍历全部 DNS record，必须避免全量扫描。
- 如果 IP 从黑名单移除，不主动恢复已删除缓存，等待后续远端 DNS 同步自然补回。

## 设计方案

### 总体架构

st-dns 在 `dns_server::schedule()` 中调用 ST Proxy console，解析 `proxy blacklist` 输出后交给 `dns_record_manager`。`dns_record_manager` 维护当前黑名单内存集合，并只对新增进入黑名单的 IP 执行缓存清理。

清理依赖 st-dns 已有反向索引 `reverse`：添加 DNS 记录时已经写入 `IP -> domains`。因此清理时从黑名单 IP 反查受影响域名，只读取这些域名的 DNS record 并删除目标 IP，避免扫描全部缓存。

### 数据流

```text
dns_server::schedule()
  -> st::command::proxy::get_blacklist_ips()
  -> parse_blacklist_ips(proxy blacklist output)
  -> dns_record_manager::sync_blacklist_ips(new_ips)
  -> diff added_ips = new_ips - current_ips
  -> remove_blacklist_ips(added_ips)
  -> reverse_resolve(ip) finds domains
  -> remove_ip_from_domain(domain, ip)
```

### ST Proxy 输出解析

`proxy blacklist` 当前输出格式为每行一个黑名单 IP，后面可跟反向域名信息：

```text
1.1.1.1\texample.com,www.example.com
2.2.2.2
```

st-dns 只读取每行第一列作为 IPv4 地址。无效 IP 跳过并记录 warning。

### DNS cache 清理

新增 `dns_record_manager::remove_blacklist_ips(const unordered_set<uint32_t>& ips)`：

1. 遍历新增黑名单 IP。
2. 调用 `reverse_resolve(ip)` 获取域名列表。
3. 对每个域名读取 `records` protobuf。
4. 在每个 server record 的 `ips` 中删除该 IP。
5. 如果某个 server record 删除后没有 IP，则删除该 server record。
6. 如果域名下没有任何 server record，则删除整个域名缓存。
7. 删除该黑名单 IP 的 reverse entry，避免后续重复读取脏反向索引。

反向索引可能已有历史脏数据。清理时以实际 DNS record 内容为准；如果反查到的域名不存在或不包含目标 IP，直接跳过。

### 返回前兜底过滤

`dns_record_manager` 提供线程安全的内存黑名单集合：

- `set_blacklist_ips()` 用于测试和直接覆盖。
- `sync_blacklist_ips()` 用于生产同步并计算新增 IP。
- `get_blacklist_ips()` / `is_blacklist_ip()` 用于查询。

`dns_record_manager::transform()` 和 `dns_record::transform()` 在生成返回记录时跳过黑名单 IP。这样即使缓存清理尚未完成，或 reverse 索引漏掉某些域名，也不会返回已知黑名单 IP。

### 错误处理

- `proxy blacklist` 调用成功：更新内存黑名单并清理新增 IP。
- `proxy blacklist` 调用失败：记录错误，不更新内存黑名单，继续使用上一轮成功同步的黑名单。
- 黑名单为空且调用成功：清空内存黑名单；已物理删除的缓存不恢复。

### 配置

本功能不新增配置项。ST Proxy console 仍使用现有硬编码地址 `127.0.0.1:5858`，同步周期复用 `dns_server::schedule()` 当前 5 秒周期。

### OpenWrt 包同步

本项目作为 OpenWrt package 提供。由于 OpenWrt 包从 st-dns git 源构建，核心代码变更随源码发布；本次不需要修改 OpenWrt 默认配置。仍需检查 `../home-openwrt/codingdie-packages/packages/st-dns`，确认无需同步额外配置或补丁。

## 测试计划

新增或更新 `src/test/unit/cache_management_test.cpp`：

- `parse_proxy_blacklist_ips`：验证 `proxy blacklist` 输出只解析每行第一列有效 IP。
- `remove_blacklist_ips_by_reverse_index`：验证按反向索引清理黑名单 IP，不依赖全量扫描。
- `resolve_filters_blacklist_ips`：验证解析返回前兜底过滤内存黑名单 IP。

验证命令：

```bash
cmake --build build --target st-dns-unit-test -j1
cd build && ./st-dns-unit-test --gtest_filter='cache_management.parse_proxy_blacklist_ips:cache_management.remove_blacklist_ips_by_reverse_index:cache_management.resolve_filters_blacklist_ips'
cd build && ctest --output-on-failure -j1
```

## 非目标

- 不实现新的 ST Proxy API。
- 不读取 ST Proxy LevelDB。
- 不为黑名单同步新增配置项。
- 不主动恢复从缓存中物理删除的 IP。
