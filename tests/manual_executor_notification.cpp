#include <tmc/aw_resume_on.hpp>
#include <tmc/current.hpp>
#include <tmc/ex_manual_st.hpp>
#include <tmc/sync.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iterator>
#include <thread>

namespace {
void Check(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "line %d: %s\n", line, expression);
    std::abort();
  }
}
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

struct NotificationProbe {
  std::atomic<std::size_t> count = 0;
  // Used only when no other consumer can remove work before observation.
  tmc::ex_manual_st* observe_publication = nullptr;
  std::atomic<bool> published = true;
};

void Notify(void* userdata) noexcept {
  auto& probe = *static_cast<NotificationProbe*>(userdata);
  if (probe.observe_publication && probe.observe_publication->empty()) {
    probe.published.store(false);
  }
  probe.count.fetch_add(1, std::memory_order_release);
}

void Init(tmc::ex_manual_st& executor) {
#ifndef TMC_PRIORITY_COUNT
  executor.set_priority_count(2);
#endif
  executor.init();
}

struct Increment {
  std::atomic<std::size_t>* value;
  void operator()() const noexcept { value->fetch_add(1, std::memory_order_relaxed); }
};

// Run both outside the executor and inside a priority-1 task. In the latter
// case posting priority 0 must exercise notify_n's successful priority CAS.
void CheckPostPaths(
  tmc::ex_manual_st& executor, NotificationProbe& probe,
  std::atomic<std::size_t>& executed
) {
  auto expected = probe.count.load();
  for (std::size_t hint : {std::size_t{0}, std::size_t{1}}) {
    tmc::post(executor, Increment{&executed}, 0, hint);
    CHECK(probe.count.load() == ++expected);
    std::array<Increment, 2> bulk{{{&executed}, {&executed}}};
    tmc::post_bulk(
      executor, std::make_move_iterator(bulk.begin()), std::size_t{0}, 0, hint
    );
    CHECK(probe.count.load() == expected);
    tmc::post_bulk(executor, std::make_move_iterator(bulk.begin()), bulk.size(), 0, hint);
    CHECK(probe.count.load() == ++expected);
    tmc::post(*executor.type_erased(), Increment{&executed}, 0, hint);
    CHECK(probe.count.load() == ++expected);
    std::array<Increment, 2> erased_bulk{{{&executed}, {&executed}}};
    tmc::post_bulk(
      *executor.type_erased(), std::make_move_iterator(erased_bulk.begin()),
      std::size_t{0}, 0, hint
    );
    CHECK(probe.count.load() == expected);
    tmc::post_bulk(
      *executor.type_erased(), std::make_move_iterator(erased_bulk.begin()),
      erased_bulk.size(), 0, hint
    );
    CHECK(probe.count.load() == ++expected);
  }
}

void PostingPaths() {
  NotificationProbe probe;
  std::atomic<std::size_t> executed = 0;
  tmc::ex_manual_st executor;
  probe.observe_publication = &executor;
  executor.set_post_notification(&Notify, &probe);
  Init(executor);
  CheckPostPaths(executor, probe, executed);
  CHECK(executed.load() == 0);
  CHECK(executor.run_all() == 12);
  CHECK(executed.load() == 12);

  tmc::post(executor, [&] { CheckPostPaths(executor, probe, executed); }, 1);
  CHECK(executor.run_one());
  CHECK(probe.count.load() == 17);
  CHECK(executor.run_all() == 12);
  CHECK(executed.load() == 24);
  CHECK(probe.published.load());
}

tmc::task<void> SwitchExecutors(
  tmc::ex_manual_st& first, tmc::ex_manual_st& second, NotificationProbe& first_probe,
  std::atomic<std::size_t>& visits
) {
  CHECK(tmc::current_executor() == first.type_erased());
  visits.fetch_add(1);
  const auto before_inline_switch = first_probe.count.load();
  co_await tmc::resume_on(first);
  CHECK(first_probe.count.load() == before_inline_switch);
  co_await tmc::resume_on(second);
  CHECK(tmc::current_executor() == second.type_erased());
  visits.fetch_add(1);
  co_await tmc::resume_on(first);
  CHECK(tmc::current_executor() == first.type_erased());
  visits.fetch_add(1);
}

void ExecutorSwitching() {
  NotificationProbe first_probe, second_probe;
  std::atomic<std::size_t> visits = 0;
  tmc::ex_manual_st first, second;
  first.set_post_notification(&Notify, &first_probe);
  second.set_post_notification(&Notify, &second_probe);
  Init(first);
  Init(second);
  tmc::post(first, SwitchExecutors(first, second, first_probe, visits));
  CHECK(first.run_one());
  CHECK(visits.load() == 1);
  CHECK(second_probe.count.load() == 1);
  CHECK(second.run_one());
  CHECK(visits.load() == 2);
  CHECK(first_probe.count.load() == 2);
  CHECK(first.run_one());
  CHECK(visits.load() == 3);
  CHECK(first.empty());
  CHECK(second.empty());
}

void ConcurrentProducers() {
  NotificationProbe probe;
  tmc::ex_manual_st executor;
  executor.set_post_notification(&Notify, &probe);
  Init(executor);
  std::atomic<std::size_t> executed = 0;
  std::array<std::thread, 4> producers;
  for (auto& producer : producers) {
    producer = std::thread([&] {
      for (std::size_t i = 0; i < 64; ++i) {
        tmc::post(executor, Increment{&executed});
      }
    });
  }
  for (auto& producer : producers)
    producer.join();
  CHECK(probe.count.load() == 256);
  CHECK(executor.run_all() == 256);
  CHECK(executed.load() == 256);
}

void NullHookAndReinitialization() {
  NotificationProbe first, second;
  std::atomic<std::size_t> executed = 0;
  tmc::ex_manual_st executor;
  Init(executor);
  tmc::post(executor, Increment{&executed});
  CHECK(executor.run_all() == 1);
  executor.teardown();
  executor.set_post_notification(nullptr, &first);
  Init(executor);
  tmc::post(executor, Increment{&executed});
  CHECK(executor.run_all() == 1);
  CHECK(first.count.load() == 0);
  executor.teardown();
  executor.set_post_notification(&Notify, &first);
  Init(executor);
  tmc::post(executor, Increment{&executed});
  CHECK(executor.run_all() == 1);
  CHECK(first.count.load() == 1);
  executor.teardown();
  Init(executor); // Teardown must have cleared both callback fields.
  tmc::post(executor, Increment{&executed});
  CHECK(executor.run_all() == 1);
  CHECK(first.count.load() == 1);
  executor.teardown();
  executor.set_post_notification(&Notify, &second);
  Init(executor);
  tmc::post(executor, Increment{&executed});
  CHECK(executor.run_all() == 1);
  CHECK(second.count.load() == 1);
  CHECK(first.count.load() == 1);
  CHECK(executed.load() == 5);
}

struct BlockedNotification {
  std::atomic<bool> entered = false;
  std::atomic<bool> release = false;
};

void BlockNotify(void* userdata) noexcept {
  auto& probe = *static_cast<BlockedNotification*>(userdata);
  probe.entered.store(true);
  probe.entered.notify_all();
  probe.release.wait(false);
}

void TeardownWaitsForNotification() {
  BlockedNotification probe;
  std::atomic<std::size_t> executed = 0;
  tmc::ex_manual_st executor;
  executor.set_post_notification(&BlockNotify, &probe);
  Init(executor);
  std::thread producer([&] { tmc::post(executor, Increment{&executed}); });
  probe.entered.wait(false);
  // The producer has published and entered the guarded callback. Consume the
  // coroutine before teardown, so no queued coroutine frame is abandoned.
  CHECK(executor.run_one());
  CHECK(executed.load() == 1);
  std::promise<void> starting;
  auto started = starting.get_future();
  auto stopped = std::async(std::launch::async, [&] {
    starting.set_value();
    executor.teardown();
  });
  started.wait();
  CHECK(stopped.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
  probe.release.store(true);
  probe.release.notify_all();
  producer.join();
  CHECK(stopped.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
  stopped.get();
}
} // namespace

int main() {
  PostingPaths();
  ExecutorSwitching();
  ConcurrentProducers();
  NullHookAndReinitialization();
  TeardownWaitsForNotification();
}
