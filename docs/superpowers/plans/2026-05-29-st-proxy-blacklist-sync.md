# ST Proxy Blacklist Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build ST Proxy IP blacklist synchronization so st-dns stops returning blacklisted A-record IPs and incrementally removes them from cache.

**Architecture:** Reuse `dns_server::schedule()` to call ST Proxy console command `proxy blacklist` every 5 seconds. Store the current blacklist in `dns_record_manager`, clean only newly blacklisted IPs via the existing reverse index `IP -> domains`, and filter blacklisted IPs again when transforming records for responses.

**Tech Stack:** C++11, Boost.Asio, protobuf-lite generated `records` map, LevelDB-backed `disk_kv`, GoogleTest, existing `st::console::client` UDP console.

---

## File Structure

- Modify `src/common/main/command/proxy_command.h`: add parser and ST Proxy console client for `proxy blacklist`.
- Modify `src/core/dns_record_manager.h`: declare blacklist memory state and cache-cleaning APIs.
- Modify `src/core/dns_record_manager.cpp`: implement blacklist state, reverse-index cleanup, and response-time filtering.
- Modify `src/core/dns_server.cpp`: call blacklist sync from existing `dns_server::schedule()`.
- Modify `src/test/unit/cache_management_test.cpp`: add tests for parsing, reverse-index cleanup, and response-time filtering.
- Create or update `docs/features/F002-2026-05-29.md`: document the feature and mark it pending review.
- Modify `docs/FEATURES.md`: add F002 to the feature index.
- Inspect `../home-openwrt/codingdie-packages/packages/st-dns`: confirm whether OpenWrt package needs config or packaging changes.

## Task 1: Add Failing Tests for Blacklist Parsing and DNS Cache Behavior

**Files:**
- Modify: `src/test/unit/cache_management_test.cpp`

- [ ] **Step 1: Add includes needed by tests**

Add `command/proxy_command.h` after existing project includes:

```cpp
#include "dns_record_manager.h"
#include "config.h"
#include "command/proxy_command.h"
#include "st.h"
```

- [ ] **Step 2: Add test for parsing `proxy blacklist` output**

Append this test to `src/test/unit/cache_management_test.cpp`:

```cpp
// 测试 ST Proxy 黑名单输出解析，只取每行第一列 IP
TEST_F(cache_management, parse_proxy_blacklist_ips) {
    string response = "1.1.1.1\texample.com,www.example.com\n"
                      "invalid-ip\tbad.example.com\n"
                      "2.2.2.2\n";

    auto ips = st::command::proxy::parse_blacklist_ips(response);

    ASSERT_EQ(2, ips.size());
    ASSERT_TRUE(ips.find(st::utils::ipv4::str_to_ip("1.1.1.1")) != ips.end());
    ASSERT_TRUE(ips.find(st::utils::ipv4::str_to_ip("2.2.2.2")) != ips.end());
}
```

- [ ] **Step 3: Add test for reverse-index cache cleanup**

Append this test to `src/test/unit/cache_management_test.cpp`:

```cpp
// 测试按反向索引清理黑名单 IP，不需要全量扫描 DNS 缓存
TEST_F(cache_management, remove_blacklist_ips_by_reverse_index) {
    dns_record_manager::uniq().clear();
    dns_record_manager::uniq().set_blacklist_ips({});

    uint32_t blocked_ip = st::utils::ipv4::str_to_ip("1.1.1.1");
    uint32_t keep_ip = st::utils::ipv4::str_to_ip("2.2.2.2");
    vector<uint32_t> ips = {blocked_ip, keep_ip};

    dns_record_manager::uniq().add("blacklist-clean-test.com", ips, "8_8_8_8_53", 600);

    dns_record_manager::uniq().remove_blacklist_ips({blocked_ip});

    auto records = dns_record_manager::uniq().get_dns_record_list("blacklist-clean-test.com");
    ASSERT_FALSE(records.empty());
    bool found_keep_ip = false;
    for (const auto &record : records) {
        for (auto ip : record.ips) {
            ASSERT_NE(blocked_ip, ip);
            if (ip == keep_ip) {
                found_keep_ip = true;
            }
        }
    }
    ASSERT_TRUE(found_keep_ip);
}
```

- [ ] **Step 4: Add test for response-time blacklist filtering**

Append this test to `src/test/unit/cache_management_test.cpp`:

```cpp
// 测试解析返回前兜底过滤内存黑名单
TEST_F(cache_management, resolve_filters_blacklist_ips) {
    dns_record_manager::uniq().clear();
    dns_record_manager::uniq().set_blacklist_ips({});

    uint32_t blocked_ip = st::utils::ipv4::str_to_ip("3.3.3.3");
    uint32_t keep_ip = st::utils::ipv4::str_to_ip("4.4.4.4");
    vector<uint32_t> ips = {blocked_ip, keep_ip};

    dns_record_manager::uniq().add("blacklist-filter-test.com", ips, "8_8_8_8_53", 600);
    dns_record_manager::uniq().set_blacklist_ips({blocked_ip});

    auto record = dns_record_manager::uniq().resolve("blacklist-filter-test.com");

    ASSERT_EQ(1, record.ips.size());
    ASSERT_EQ(keep_ip, record.ips[0]);

    dns_record_manager::uniq().set_blacklist_ips({});
}
```

- [ ] **Step 5: Run test build and verify RED**

Run:

```bash
cmake --build build --target st-dns-unit-test -j1
```

Expected: build fails because `st::command::proxy::parse_blacklist_ips`, `dns_record_manager::set_blacklist_ips`, and `dns_record_manager::remove_blacklist_ips` are not declared.

## Task 2: Implement ST Proxy Blacklist Parser and Console Fetcher

**Files:**
- Modify: `src/common/main/command/proxy_command.h`

- [ ] **Step 1: Add unordered_set include**

Add this include near the top of `src/common/main/command/proxy_command.h`:

```cpp
#include <unordered_set>
```

- [ ] **Step 2: Add `parse_blacklist_ips` and `get_blacklist_ips`**

Add these functions inside `namespace st::command::proxy`, after `get_ip_available_proxy_areas`:

```cpp
static unordered_set<uint32_t> parse_blacklist_ips(const string &response) {
    unordered_set<uint32_t> result;
    auto lines = st::utils::strutils::split(response, "\n");
    for (auto line : lines) {
        st::utils::strutils::trim(line);
        if (line.empty()) {
            continue;
        }
        auto columns = st::utils::strutils::split(line, "\t", 0, 1);
        if (columns.empty()) {
            continue;
        }
        string ip_str = st::utils::strutils::trim(std::move(columns[0]));
        uint32_t ip = st::utils::ipv4::str_to_ip(ip_str);
        if (ip > 0) {
            result.emplace(ip);
        } else {
            logger::WARN << "skip invalid proxy blacklist ip" << ip_str << END;
        }
    }
    return result;
}

static pair<bool, unordered_set<uint32_t>> get_blacklist_ips() {
    auto begin = time::now();
    auto result = st::console::client::command("127.0.0.1", 5858, "proxy blacklist", 1000);
    apm_logger::perf("get-proxy-blacklist", {}, st::utils::time::now() - begin);
    if (!result.first) {
        logger::ERROR << "get proxy blacklist error!" << result.second << END;
        return make_pair(false, unordered_set<uint32_t>{});
    }
    return make_pair(true, parse_blacklist_ips(result.second));
}
```

- [ ] **Step 3: Run parser test only and verify partial GREEN**

Run:

```bash
cmake --build build --target st-dns-unit-test -j1
cd build && ./st-dns-unit-test --gtest_filter='cache_management.parse_proxy_blacklist_ips'
```

Expected: parser test passes; other new tests may still fail to build or fail until Task 3 is complete.

## Task 3: Add Blacklist State and Reverse-Index Cleanup APIs

**Files:**
- Modify: `src/core/dns_record_manager.h`
- Modify: `src/core/dns_record_manager.cpp`

- [ ] **Step 1: Add private blacklist state to header**

In `class dns_record_manager`, add private fields near `schedule_timer`:

```cpp
mutex blacklist_lock;
unordered_set<uint32_t> blacklist_ips;
```

- [ ] **Step 2: Declare public blacklist APIs**

In the public section of `class dns_record_manager`, add:

```cpp
void set_blacklist_ips(const unordered_set<uint32_t> &ips);

void sync_blacklist_ips(const unordered_set<uint32_t> &ips);

unordered_set<uint32_t> get_blacklist_ips();

bool is_blacklist_ip(uint32_t ip);

void remove_blacklist_ips(const unordered_set<uint32_t> &ips);
```

- [ ] **Step 3: Declare private domain cleanup helper**

In the private section of `class dns_record_manager`, add:

```cpp
bool remove_ip_from_domain(const string &domain, uint32_t ip);
```

- [ ] **Step 4: Implement blacklist state methods**

Add these implementations in `src/core/dns_record_manager.cpp` after `add_reverse_record`:

```cpp
void dns_record_manager::set_blacklist_ips(const unordered_set<uint32_t> &ips) {
    lock_guard<mutex> lock(blacklist_lock);
    blacklist_ips = ips;
}

unordered_set<uint32_t> dns_record_manager::get_blacklist_ips() {
    lock_guard<mutex> lock(blacklist_lock);
    return blacklist_ips;
}

bool dns_record_manager::is_blacklist_ip(uint32_t ip) {
    lock_guard<mutex> lock(blacklist_lock);
    return blacklist_ips.find(ip) != blacklist_ips.end();
}

void dns_record_manager::sync_blacklist_ips(const unordered_set<uint32_t> &ips) {
    unordered_set<uint32_t> added_ips;
    {
        lock_guard<mutex> lock(blacklist_lock);
        for (const auto &ip : ips) {
            if (blacklist_ips.find(ip) == blacklist_ips.end()) {
                added_ips.emplace(ip);
            }
        }
        blacklist_ips = ips;
    }
    if (!added_ips.empty()) {
        remove_blacklist_ips(added_ips);
    }
}
```

- [ ] **Step 5: Implement reverse-index cleanup**

Add these implementations in `src/core/dns_record_manager.cpp` after the methods from Step 4:

```cpp
void dns_record_manager::remove_blacklist_ips(const unordered_set<uint32_t> &ips) {
    uint32_t domain_count = 0;
    for (auto ip : ips) {
        auto reverse_record = reverse_resolve(ip);
        for (const auto &domain : reverse_record.domains()) {
            if (remove_ip_from_domain(domain, ip)) {
                domain_count++;
            }
        }
        reverse.erase(to_string(ip));
    }
    if (!ips.empty()) {
        logger::INFO << "remove blacklist ips from dns cache"
                     << "ip_count" << ips.size()
                     << "domain_count" << domain_count << END;
    }
}

bool dns_record_manager::remove_ip_from_domain(const string &domain, uint32_t ip) {
    string data = db.get(domain);
    if (data.empty()) {
        return false;
    }

    st::dns::proto::records records;
    records.ParseFromString(data);
    if (records.domain().empty()) {
        return false;
    }

    bool changed = false;
    auto *record_map = records.mutable_map();
    vector<string> empty_servers;
    for (auto &server_record : *record_map) {
        auto *record = &server_record.second;
        vector<uint32_t> keep_ips;
        bool server_changed = false;
        for (auto record_ip : record->ips()) {
            if (record_ip == ip) {
                server_changed = true;
            } else {
                keep_ips.emplace_back(record_ip);
            }
        }
        if (server_changed) {
            changed = true;
            record->clear_ips();
            for (auto keep_ip : keep_ips) {
                record->add_ips(keep_ip);
            }
            if (keep_ips.empty()) {
                empty_servers.emplace_back(server_record.first);
            }
        }
    }

    if (!changed) {
        return false;
    }

    for (const auto &server : empty_servers) {
        record_map->erase(server);
    }
    if (record_map->empty()) {
        db.erase(domain);
    } else {
        db.put(domain, records.SerializeAsString());
    }
    return true;
}
```

- [ ] **Step 6: Clear in-memory blacklist when clearing cache**

Update `dns_record_manager::clear()` to include:

```cpp
void dns_record_manager::clear() {
    db.clear();
    reverse.clear();
    set_blacklist_ips({});
}
```

- [ ] **Step 7: Run cleanup test and verify GREEN**

Run:

```bash
cmake --build build --target st-dns-unit-test -j1
cd build && ./st-dns-unit-test --gtest_filter='cache_management.remove_blacklist_ips_by_reverse_index'
```

Expected: `cache_management.remove_blacklist_ips_by_reverse_index` passes.

## Task 4: Filter Blacklisted IPs While Transforming Records

**Files:**
- Modify: `src/core/dns_record_manager.cpp`

- [ ] **Step 1: Filter in smart `dns_record_manager::transform`**

At the start of `dns_record_manager::transform`, after `record.domain = records.domain();`, add:

```cpp
auto blacklist_ips = dns_record_manager::uniq().get_blacklist_ips();
```

Inside the loop over `t_record.ips()`, before creating `dns_ip_record tmp`, add:

```cpp
if (blacklist_ips.find(ip) != blacklist_ips.end()) {
    continue;
}
```

- [ ] **Step 2: Filter in list `dns_record::transform`**

At the start of `dns_record::transform`, add:

```cpp
auto blacklist_ips = dns_record_manager::uniq().get_blacklist_ips();
```

Replace direct vector assignment:

```cpp
record.ips = vector<uint32_t>(item.ips().begin(), item.ips().end());
```

with:

```cpp
for (auto ip : item.ips()) {
    if (blacklist_ips.find(ip) == blacklist_ips.end()) {
        record.ips.emplace_back(ip);
    }
}
if (record.ips.empty()) {
    continue;
}
```

- [ ] **Step 3: Run filtering test and verify GREEN**

Run:

```bash
cmake --build build --target st-dns-unit-test -j1
cd build && ./st-dns-unit-test --gtest_filter='cache_management.resolve_filters_blacklist_ips'
```

Expected: `cache_management.resolve_filters_blacklist_ips` passes.

## Task 5: Sync ST Proxy Blacklist from Existing Schedule

**Files:**
- Modify: `src/core/dns_server.cpp`

- [ ] **Step 1: Call blacklist fetcher at the start of `dns_server::schedule`**

At the beginning of `void dns_server::schedule()`, before iterating `config.servers`, add:

```cpp
auto blacklist_result = st::command::proxy::get_blacklist_ips();
if (blacklist_result.first) {
    dns_record_manager::uniq().sync_blacklist_ips(blacklist_result.second);
}
```

- [ ] **Step 2: Build server target**

Run:

```bash
cmake --build build --target st-dns -j1
```

Expected: `st-dns` target builds successfully.

## Task 6: Update Feature Documentation

**Files:**
- Create: `docs/features/F002-2026-05-29.md`
- Modify: `docs/FEATURES.md`

- [ ] **Step 1: Create feature document**

Create `docs/features/F002-2026-05-29.md` with:

```markdown
# F002 - ST Proxy IP 黑名单同步

**创建日期**: 2026-05-29
**最后更新**: 2026-05-29

---

## 功能概述

st-dns 定时从 ST Proxy 获取 IP 黑名单，并避免继续向客户端返回这些 IP。

---

## 状态信息

| 项目 | 内容 |
|------|------|
| **状态** | 👀 待审核 |
| **优先级** | 高 |
| **负责人** | codingdie |
| **实际完成** | 2026-05-29 |

---

## 功能描述

### 核心功能

- 复用 `dns_server::schedule()` 每 5 秒通过 ST Proxy console 命令 `proxy blacklist` 同步黑名单。
- 解析 `proxy blacklist` 输出时只读取每行第一列 IP，兼容 `IP\t域名列表` 格式。
- st-dns 在内存中维护当前黑名单集合。
- 对新增黑名单 IP，基于现有反向索引 `IP -> domains` 增量清理 DNS 缓存，不全量扫描缓存。
- 在 DNS record 转换为返回结果时再次过滤黑名单 IP，兜底处理并发窗口和未清理记录。

### 功能边界

- ✅ 包含 IPv4 A 记录缓存中的黑名单 IP 清理和返回前过滤。
- ✅ 包含 ST Proxy console 调用失败时保留上一轮黑名单，避免错误清空。
- ❌ 不主动恢复已物理删除的 IP；IP 从黑名单移除后等待后续远端解析自然补回。
- ❌ 不新增配置项，跟随现有 5 秒 schedule 周期。
```

- [ ] **Step 2: Update feature index**

In `docs/FEATURES.md`, update `最后更新` to `2026-05-29` and add this row:

```markdown
| F002 | ST Proxy IP 黑名单同步 | 👀 待审核 | 高 | 2026-05-29 | [features/F002-2026-05-29.md](features/F002-2026-05-29.md) |
```

- [ ] **Step 3: Inspect OpenWrt package**

Run:

```bash
find ../home-openwrt/codingdie-packages/packages/st-dns -maxdepth 3 -type f -print
cat ../home-openwrt/codingdie-packages/packages/st-dns/files/config.json
```

Expected: no OpenWrt package file requires changes because the feature adds no config and package builds from the st-dns source git.

## Task 7: Run Verification

**Files:**
- No file edits.

- [ ] **Step 1: Run focused tests**

Run:

```bash
cmake --build build --target st-dns-unit-test -j1
cd build && ./st-dns-unit-test --gtest_filter='cache_management.parse_proxy_blacklist_ips:cache_management.remove_blacklist_ips_by_reverse_index:cache_management.resolve_filters_blacklist_ips'
```

Expected: all 3 focused tests pass.

- [ ] **Step 2: Run full C++ test suite serially**

Run:

```bash
cd build && ctest --output-on-failure -j1
```

Expected: all discovered tests pass. If a test fails, capture the failure output and fix the failing behavior with a new failing test before changing production code.

- [ ] **Step 3: Check git diff**

Run:

```bash
git status --short
git diff -- src/common/main/command/proxy_command.h src/core/dns_record_manager.h src/core/dns_record_manager.cpp src/core/dns_server.cpp src/test/unit/cache_management_test.cpp docs/FEATURES.md docs/features/F002-2026-05-29.md
```

Expected: diff only contains blacklist sync feature changes and documentation.

## Self-Review

- Spec coverage: The plan covers ST Proxy console fetching, output parsing, in-memory blacklist, reverse-index cleanup, response filtering, failure behavior, tests, docs, and OpenWrt package inspection.
- Placeholder scan: No placeholders are intentionally left in this plan; every code step includes exact code snippets or exact commands.
- Type consistency: All new APIs use `unordered_set<uint32_t>` and match declarations and definitions: `set_blacklist_ips`, `sync_blacklist_ips`, `get_blacklist_ips`, `is_blacklist_ip`, `remove_blacklist_ips`, and `remove_ip_from_domain`.
