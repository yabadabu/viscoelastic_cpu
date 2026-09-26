#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

// A phase dispatcher for the simulation. The main thread publishes one
// callback and a number of chunks; workers dynamically claim chunks through
// next_job. Between phases they spin only while a simulation update is active,
// then park on the condition variable while the frame is rendered.
class ThreadPool {
public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void beginUpdate();
  void endUpdate();

  template<typename Fn>
  void dispatch(int num_jobs, Fn&& fn);

  static size_t currentWorkerIndex() {
    assert(worker_index != invalid_worker_index);
    return worker_index;
  }

private:
  using WorkCallback = void(*)(void*, int);

  static constexpr size_t invalid_worker_index = (size_t)-1;
  inline static thread_local size_t worker_index = invalid_worker_index;

  static void spinPause();
  void workerLoop(size_t index);
  int consumePhase();

  std::vector<std::thread> workers;

  // Written by the submitting thread before work_generation is released and
  // left untouched until every worker acknowledges that generation.
  WorkCallback work_callback = nullptr;
  void* work_context = nullptr;
  int published_jobs = 0;

  alignas(64) std::atomic<int> next_job{ 0 };
  alignas(64) std::atomic<int> jobs_remaining{ 0 };
  alignas(64) std::atomic<int> workers_remaining{ 0 };
  alignas(64) std::atomic<uint64_t> work_generation{ 0 };
  alignas(64) std::atomic<bool> update_active{ false };
  std::atomic<bool> stop{ false };

  std::mutex wait_mutex;
  std::condition_variable wake_workers;
};

inline void ThreadPool::spinPause() {
#if defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
  _mm_pause();
#else
  std::this_thread::yield();
#endif
}

inline ThreadPool::ThreadPool(size_t num_threads) {
  workers.reserve(num_threads);
  for (size_t i = 0; i < num_threads; ++i)
    workers.emplace_back([this, i]() { workerLoop(i); });
}

inline void ThreadPool::beginUpdate() {
  {
    // Pair state changes with the wait mutex so a worker cannot test the
    // predicate and go to sleep between this store and the notification.
    std::lock_guard<std::mutex> lock(wait_mutex);
    update_active.store(true, std::memory_order_release);
  }
  wake_workers.notify_all();
}

inline void ThreadPool::endUpdate() {
  update_active.store(false, std::memory_order_release);
}

inline int ThreadPool::consumePhase() {
  int completed_jobs = 0;
  for (;;) {
    const int job_id = next_job.fetch_add(1, std::memory_order_relaxed);
    if (job_id >= published_jobs) {
      if (completed_jobs > 0)
        jobs_remaining.fetch_sub(completed_jobs, std::memory_order_acq_rel);
      return completed_jobs;
    }
    work_callback(work_context, job_id);
    ++completed_jobs;
  }
}

inline void ThreadPool::workerLoop(size_t index) {
  worker_index = index;
  uint64_t observed_generation = 0;
  unsigned int idle_spins = 0;

  for (;;) {
    const uint64_t generation = work_generation.load(std::memory_order_acquire);
    if (generation != observed_generation) {
      observed_generation = generation;
      consumePhase();
      // acq_rel chains the workers' writes through the RMW sequence; the
      // submitting thread's acquire of zero then observes every completed job.
      workers_remaining.fetch_sub(1, std::memory_order_acq_rel);
      idle_spins = 0;
      continue;
    }

    if (update_active.load(std::memory_order_acquire)) {
      // Stay runnable without permanently starving the submitting/render
      // thread when the user selects every logical processor as a worker.
      if ((++idle_spins & 1023u) == 0)
        std::this_thread::yield();
      else
        spinPause();
      continue;
    }

    idle_spins = 0;
    std::unique_lock<std::mutex> lock(wait_mutex);
    wake_workers.wait(lock, [this, &observed_generation]() {
      return stop.load(std::memory_order_acquire)
        || update_active.load(std::memory_order_acquire)
        || work_generation.load(std::memory_order_acquire) != observed_generation;
      });

    if (stop.load(std::memory_order_acquire))
      return;
  }
}

template<typename Fn>
inline void ThreadPool::dispatch(int num_jobs, Fn&& fn) {
  if (num_jobs <= 0)
    return;

  // The previous callback may already have returned while a worker was still
  // observing its generation. Wait for those acknowledgements only when the
  // next phase actually needs to reuse the published callback fields.
  unsigned int wait_spins = 0;
  while (workers_remaining.load(std::memory_order_acquire) != 0) {
    if ((++wait_spins & 255u) == 0)
      std::this_thread::yield();
    else
      spinPause();
  }

  using FnType = typename std::remove_reference<Fn>::type;
  work_context = &fn;
  work_callback = [](void* context, int job_id) {
    (*static_cast<FnType*>(context))(job_id);
    };
  published_jobs = num_jobs;
  next_job.store(0, std::memory_order_relaxed);
  jobs_remaining.store(num_jobs, std::memory_order_relaxed);
  workers_remaining.store((int)workers.size(), std::memory_order_relaxed);

  // The release publishes the callback, context, and counters as one phase.
  // When workers are parked, hold the wait mutex across publication to avoid
  // a notification racing with the predicate-to-sleep transition.
  const bool workers_are_spinning = update_active.load(std::memory_order_acquire);
  if (workers_are_spinning) {
    work_generation.fetch_add(1, std::memory_order_release);
  }
  else {
    std::lock_guard<std::mutex> lock(wait_mutex);
    work_generation.fetch_add(1, std::memory_order_release);
  }
  if (!workers_are_spinning)
    wake_workers.notify_all();

  wait_spins = 0;
  while (jobs_remaining.load(std::memory_order_acquire) != 0) {
    if ((++wait_spins & 255u) == 0)
      std::this_thread::yield();
    else
      spinPause();
  }
}

inline ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(wait_mutex);
    update_active.store(false, std::memory_order_release);
    stop.store(true, std::memory_order_release);
  }
  wake_workers.notify_all();
  for (std::thread& worker : workers)
    worker.join();
}
