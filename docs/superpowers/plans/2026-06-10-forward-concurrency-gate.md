# Forward Task Queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an isolated forward `task_queue` so non-`A/AAAA` DNS forward requests are rejected immediately when the forward queue is full.

**Architecture:** `dns_server` owns a `st::task::queue<forward_task_queue_param>` dedicated to `forward_dns_request()`. The queue parameter follows the existing sync queue style and uses a simple `pair`, not a custom payload struct. The queue uses `max_running == max_size`, so it only accepts a fixed number of active/pending forward tasks; failed `submit()` immediately returns an empty DNS response. The limit defaults to `config.forward_max_running` (`forward_max_running` in `config.json`, default 32), while tests can override it through the constructor. `A` and `AAAA` processing paths remain unchanged.

**Tech Stack:** C++11, Boost.Asio UDP server/client, GoogleTest integration tests, CMake.

---

## File Structure

- Modify `src/core/dns_server.h`
  - Add default forward concurrency constant.
  - Add optional constructor argument for tests; production uses `config.forward_max_running`.
  - Add `forward_task_queue_param` and `forward_task_queue`.
- Modify `src/core/config.h`
  - Add `forward_max_running` with default 32.
- Modify `src/core/config.cpp`
  - Load, copy, and reset `forward_max_running`.
- Modify `src/common/main/taskquque/task_queue.h`
  - Add optional `max_size` capacity limit.
  - Preserve existing unlimited behavior when `max_size == 0`.
- Modify `src/core/dns_server.cpp`
  - Remove the previous 100ms forward timeout cap.
  - Submit forward work to `forward_task_queue`.
  - Reject immediately when `forward_task_queue.submit()` fails.
- Modify `src/test/integration/dns_server_test.cpp`
  - Replace the previous “single forward returns within 100ms” test with queue capacity behavior tests.

No commit step is included because this environment requires an explicit user request before committing.

---

### Task 1: Write Failing Forward Gate Tests

**Files:**
- Modify: `src/test/integration/dns_server_test.cpp`

- [ ] **Step 1: Replace the existing timeout test with two forward queue tests**

Use this helper and tests in `src/test/integration/dns_server_test.cpp`:

```cpp
static st::dns::protocol::udp_request build_https_query(const string &domain) {
    st::dns::protocol::udp_request request({domain});
    auto *query = request.query_zone->querys[0];
    query->data[query->domain->len] = 0x00;
    query->data[query->domain->len + 1] = 0x41;
    return request;
}

TEST(integration_timeout_tests, non_a_query_uses_upstream_timeout_when_forward_capacity_available) {
    st::dns::config::INSTANCE.load("../confs/test");
    dns_record_manager::uniq().clear();
    st::dns::config::INSTANCE.servers[0]->ip = "127.0.0.1";
    st::dns::config::INSTANCE.servers[0]->port = 1;
    st::dns::config::INSTANCE.servers[0]->timeout = 500;

    auto *server = new dns_server(st::dns::config::INSTANCE, 1);
    auto *th = new thread([=]() { server->start(); });
    server->wait_start();

    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    boost::asio::ip::udp::endpoint server_endpoint(boost::asio::ip::make_address_v4("127.0.0.1"), 5353);

    auto request = build_https_query("capacity.example.com");
    auto begin = time::now();
    socket.send_to(boost::asio::buffer(request.data, request.len), server_endpoint);

    uint8_t buffer[1024] = {0};
    boost::asio::ip::udp::endpoint response_endpoint;
    size_t response_size = socket.receive_from(boost::asio::buffer(buffer, sizeof(buffer)), response_endpoint);
    auto cost = time::now() - begin;

    server->shutdown();
    th->join();
    delete th;
    delete server;
    st::dns::config::INSTANCE.unload();

    ASSERT_GT(response_size, 0);
    ASSERT_GE(cost, 400);
    ASSERT_LE(cost, 800);
}

TEST(integration_timeout_tests, non_a_query_rejected_immediately_when_forward_concurrency_full) {
    st::dns::config::INSTANCE.load("../confs/test");
    dns_record_manager::uniq().clear();
    st::dns::config::INSTANCE.servers[0]->ip = "127.0.0.1";
    st::dns::config::INSTANCE.servers[0]->port = 1;
    st::dns::config::INSTANCE.servers[0]->timeout = 500;

    auto *server = new dns_server(st::dns::config::INSTANCE, 1);
    auto *th = new thread([=]() { server->start(); });
    server->wait_start();

    boost::asio::io_context io_context;
    boost::asio::ip::udp::endpoint server_endpoint(boost::asio::ip::make_address_v4("127.0.0.1"), 5353);
    boost::asio::ip::udp::socket slow_socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    boost::asio::ip::udp::socket rejected_socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));

    auto slow_request = build_https_query("slow.example.com");
    slow_socket.send_to(boost::asio::buffer(slow_request.data, slow_request.len), server_endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto rejected_request = build_https_query("rejected.example.com");
    auto begin = time::now();
    rejected_socket.send_to(boost::asio::buffer(rejected_request.data, rejected_request.len), server_endpoint);

    uint8_t rejected_buffer[1024] = {0};
    boost::asio::ip::udp::endpoint rejected_endpoint;
    size_t rejected_size = rejected_socket.receive_from(boost::asio::buffer(rejected_buffer, sizeof(rejected_buffer)), rejected_endpoint);
    auto rejected_cost = time::now() - begin;

    uint8_t slow_buffer[1024] = {0};
    boost::asio::ip::udp::endpoint slow_endpoint;
    size_t slow_size = slow_socket.receive_from(boost::asio::buffer(slow_buffer, sizeof(slow_buffer)), slow_endpoint);

    server->shutdown();
    th->join();
    delete th;
    delete server;
    st::dns::config::INSTANCE.unload();

    ASSERT_GT(rejected_size, 0);
    ASSERT_GT(slow_size, 0);
    ASSERT_LE(rejected_cost, 150);
}
```

- [ ] **Step 2: Run the focused tests and verify they fail before implementation**

Run:

```bash
cmake --build build --target st-dns-integration-test -j1
./st-dns-integration-test --gtest_filter=integration_timeout_tests.non_a_query_uses_upstream_timeout_when_forward_capacity_available:integration_timeout_tests.non_a_query_rejected_immediately_when_forward_concurrency_full
```

Expected before implementation:

- The capacity-available test should fail to compile until the constructor overload exists.
- After adding only the constructor signature without queue capacity behavior, the concurrency-full test should fail because the second request waits around 500ms instead of returning within 150ms.

---

### Task 2: Implement Forward Task Queue Capacity

**Files:**
- Modify: `src/core/dns_server.h`
- Modify: `src/core/dns_server.cpp`
- Modify: `src/common/main/taskquque/task_queue.h`

- [ ] **Step 1: Add optional queue capacity to `task_queue.h`**

Add `max_size` to the queue class:

```cpp
uint32_t max_size = 0;
```

Extend the constructor with a defaulted `max_size`:

```cpp
explicit queue(string name, uint32_t speed, uint32_t max_running,
               const std::function<void(st::task::priority_task<input>)> &executor,
               uint32_t max_size = 0)
    : name(std::move(name)), ic(), iw(new io_context::work(ic)), th([this]() { ic.run(); }),
      generate_key_timer(ic), schedule_timer(ic), executor(executor), max_qps(speed),
      max_running(max_running), max_size(max_size), running(0) {
    schedule_generate_key();
    schedule_dispatch_task();
};
```

Reject submissions when the bounded queue is full:

```cpp
bool submit(const st::task::priority_task<input> &task) {
    std::lock_guard<std::mutex> lg(mutex);
    if (max_size > 0 && tasks.size() >= max_size) {
        return false;
    }
    if (task.pk.empty() || task_pks.emplace(task.pk).second) {
        tasks.emplace(make_pair(task.id, task));
        p_queue.push(make_pair(task.id, task.priority));
        return true;
    } else {
        return false;
    }
}
```

- [ ] **Step 2: Update `dns_server.h`**

Add the forward limit constant, queue param alias, and constructor/member declarations:

```cpp
static const uint64_t MAX_PRIORITY = 100;
static const uint32_t DEFAULT_FORWARD_MAX_RUNNING = 32;
#define forward_task_queue_param pair<st::dns::session *, std::function<void(st::dns::session *)>>

class dns_server {
public:
    explicit dns_server(st::dns::config &config, uint32_t forward_max_running = 0);
```

Add private members:

```cpp
    uint32_t forward_max_running;
    st::task::queue<forward_task_queue_param> forward_task_queue;
```

- [ ] **Step 3: Update constructor initialization in `dns_server.cpp`**

Change the constructor signature and initializer list:

```cpp
dns_server::dns_server(st::dns::config &config, uint32_t forward_max_running) : rid(time::now()),
                                                  config(config),
                                                  counter(0),
                                                  forward_max_running(forward_max_running),
                                                  accepting_remote_sync_callbacks(std::make_shared<std::atomic_bool>(true)),
                                                  sync_remote_record_task_queue(
                                                          "st-dns-record-sync-task",
                                                          100,
                                                          100,
                                                          [this](const st::task::priority_task<pair<string, remote_dns_server *>> &task) {
                                                              auto domain = task.get_input().first;
                                                              auto server = task.get_input().second;
                                                              auto accepting_callbacks = accepting_remote_sync_callbacks;
                                                              sync_dns_record_from_remote(domain, [this, accepting_callbacks, task](const dns_record &record) {
                                                                  if (!accepting_callbacks->load()) {
                                                                      return;
                                                                  }
                                                                  sync_remote_record_task_queue.complete(task);
                                                              }, server);
                                                          }),
                                                  forward_task_queue(
                                                          "st-dns-forward-task",
                                                          forward_max_running,
                                                          forward_max_running,
                                                          [this](const st::task::priority_task<forward_task_queue_param> &task) {
                                                              auto param = task.get_input();
                                                              auto *session = param.first;
                                                              auto complete_handler = param.second;
                                                              auto *server = select_forward_udp_server();
                                                              dns_client::uniq().forward_udp(session->request,
                                                                                             server->ip,
                                                                                             server->port,
                                                                                             server->timeout,
                                                                                             [this, task, param](udp_response *response) {
                                                                                                 auto *session = param.first;
                                                                                                 auto complete_handler = param.second;
                                                                                                 forward_task_queue.complete(task);
                                                                                                 session->response = response;
                                                                                                 complete_handler(session);
                                                                                             });
                                                          },
                                                          forward_max_running)
```

- [ ] **Step 4: Replace `forward_dns_request()`**

Use this behavior:

```cpp
void dns_server::forward_dns_request(session *session, const std::function<void(st::dns::session *session)> &complete_handler) {
    remote_dns_server *server = nullptr;
    for (auto &it : config.servers) {
        if (it->type == "UDP") {
            server = it;
            break;
        }
    }
    if (server == nullptr) {
        session->logger.add_dimension("forward_status", "no_udp_server");
        complete_handler(session);
        return;
    }

    st::task::priority_task<forward_task_queue_param> task(
            make_pair(session, complete_handler),
            MAX_PRIORITY,
            to_string(session->get_id()));
    if (!forward_task_queue.submit(task)) {
        session->logger.add_dimension("forward_status", "rejected");
        complete_handler(session);
        return;
    }

    session->logger.add_dimension("forward_status", "accepted");
}
```

- [ ] **Step 5: Remove previous 100ms forward cap**

Delete any local `MAX_CLIENT_RESPONSE_TIMEOUT_MS` constant that was introduced only for forward capping. Keep the `A` query path waiting exactly 100ms as before:

```cpp
timer->expires_from_now(boost::posix_time::milliseconds(100));
```

- [ ] **Step 6: Run focused tests and verify they pass**

Run:

```bash
cmake --build build --target st-dns-integration-test -j1
./st-dns-integration-test --gtest_filter=integration_timeout_tests.non_a_query_uses_upstream_timeout_when_forward_capacity_available:integration_timeout_tests.non_a_query_rejected_immediately_when_forward_concurrency_full
```

Expected:

- Both tests pass.
- The capacity-available test takes roughly 500ms request time.
- The concurrency-full rejected request completes within 150ms.

---

### Task 3: Full Verification

**Files:**
- No source edits.

- [ ] **Step 1: Build all targets**

Run:

```bash
cmake --build build -j1
```

Expected:

- Build completes successfully.
- Existing warnings may remain, but no new errors.

- [ ] **Step 2: Run all tests serially**

Run:

```bash
cd build
ctest --output-on-failure -j1
```

Expected:

- All registered tests pass.

- [ ] **Step 3: Inspect final diff**

Run:

```bash
git diff -- src/core/dns_server.h src/core/dns_server.cpp src/test/integration/dns_server_test.cpp docs/superpowers/specs/2026-06-10-forward-concurrency-gate-design.md docs/superpowers/plans/2026-06-10-forward-concurrency-gate.md
```

Expected:

- Diff only contains forward task queue capacity, tests, and design/plan docs.
- No unrelated formatting churn.
