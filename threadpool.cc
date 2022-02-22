#include <benchmark/benchmark.h>
#include <iostream>
#include <vector>
#include <future>
#include <omp.h>

#define RANGE_LOW 10 * 1024
#define RANGE_HIGH (10*1024*1024)
#define RANGE_MULTI 8

#define THREAD_LOW 2
#define THREAD_HIGH 8
#define THREAD_MULTI 2

// #define RANGE_LOW 256 * 1024
// #define RANGE_HIGH 256 * 1024
// #define RANGE_MULTI 2
// 
// #define THREAD_LOW 2
// #define THREAD_HIGH 4
// #define THREAD_MULTI 2


class ThreadPoolFixture : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State & state) {
    data = new float[state.range(0)];
    // warmup by touching each item
    for (int i = 0; i < state.range(0); i++) {
      data[i] = 0.0f;
    }
  }
  void TearDown(const ::benchmark::State & state) {
    delete[] data;
  }
  float *data;
};

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_raw)(benchmark::State & state) {
    for (auto _ : state) {
      for (int i = 0; i < state.range(0); i++) {
        data[i] = 0.1 * i;
      }
    }
}
BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_raw)->RangeMultiplier(RANGE_MULTI)->Range(RANGE_LOW, RANGE_HIGH);

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  for (auto _ : state) {
    for (int i = 0; i < state.range(0); i++) {
      f(i);
    }
  }
}
BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda)->RangeMultiplier(RANGE_MULTI)->Range(RANGE_LOW, RANGE_HIGH);

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda_wrapped)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  auto wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  for (auto _ : state) {
    for (int i = 0; i < state.range(1); i++) {
      wrapped_f(i);
    }
  }
}

BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda_wrapped)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda_wrapped_async_deferred)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  auto wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  for (auto _ : state) {
    std::vector<std::future<void>> futures;
    for (int i = 0; i < state.range(1); i++) {
      futures.push_back(std::async(std::launch::deferred, wrapped_f, i));
    }
    for (auto & f : futures) f.wait();
  }
}

BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda_wrapped_async_deferred)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });
BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda_wrapped_async_threaded)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  auto wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  for (auto _ : state) {
    std::vector<std::future<void>> futures;
    for (int i = 0; i < state.range(1); i++) {
      futures.push_back(std::async(std::launch::async, wrapped_f, i));
    }
    for (auto & f : futures) f.wait();
  }
}

BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda_wrapped_async_threaded)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda_wrapped_pool_threaded_spin)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  auto wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  std::vector<std::atomic_int> status(state.range(1));
  std::vector<std::function<void(int)>> payloads(state.range(1), wrapped_f);
  std::atomic_int thread_exit = 0;

  std::vector<std::thread> thread_pool;
  for (int i = 1; i < state.range(1); i++) {
    status[i] = 0;
    thread_pool.emplace_back([&,i]() {
      for (; !thread_exit; ) {
        if (status[i] == 0) continue;
        payloads[i](i);
        status[i] = 0;
      }
    });
  }

  for (auto _ : state) {
    for (int i = 1; i < state.range(1); i++) {
      status[i] = 1;
    }
    wrapped_f(0);
    for (int i = 1; i < state.range(1); i++) {
      for (; status[i] == 1;) {}
    }
  }
  thread_exit = 1;
  for (auto & t : thread_pool) t.join();
}

BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda_wrapped_pool_threaded_spin)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_lambda_wrapped_pool_threaded_condition_var)(benchmark::State & state) {
  auto f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  auto wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  std::vector<int> status(state.range(1), 0);
  std::vector<std::function<void(int)>> payloads(state.range(1), wrapped_f);
  int thread_exit = 0;
  std::condition_variable cv;
  std::condition_variable cv2;
  std::mutex cv_m;

  std::vector<std::thread> thread_pool;
  for (int i = 1; i < state.range(1); i++) {
    status[i] = 0;
    thread_pool.emplace_back([&,i]() {
      for (;;) {
        {
          std::unique_lock<std::mutex> lk(cv_m);
          cv.wait(lk, [&]() { return (thread_exit == 1) || (status[i] == 1); });
        }
        if (thread_exit) break;
        payloads[i](i);
        {
          std::unique_lock<std::mutex> lk(cv_m);
          status[i] = 0;
        }
        cv2.notify_one();
      }
    });
  }

  for (auto _ : state) {
    {
      std::lock_guard<std::mutex> lock(cv_m);
      for (int i = 1; i < state.range(1); i++) {
        status[i] = 1;
      }
    }
    cv.notify_all();
    wrapped_f(0);
    {
      std::unique_lock<std::mutex> lk(cv_m);
      cv2.wait(lk, [&]() {
        int done = 1;
        for (int i = 1; i < state.range(1); i++) done &= ~status[i];
        return done;
      });
    }
  }
  {
    std::lock_guard<std::mutex> lock(cv_m);
    thread_exit = 1;
  }
  cv.notify_all();
  for (auto & t : thread_pool) t.join();
}

BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_lambda_wrapped_pool_threaded_condition_var)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_std_function)(benchmark::State & state) {
  std::function<void(int)> f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  for (auto _ : state) {
    for (int i = 0; i < state.range(0); i++) {
      f(i);
    }
  }
}
BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_std_function)->RangeMultiplier(RANGE_MULTI)->Range(RANGE_LOW, RANGE_HIGH);

BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_std_function_wrapped)(benchmark::State & state) {
  std::function<void(int)> f = [&](int i) -> void {
    data[i] = 0.1 * i;
  };
  std::function<void(int)> wrapped_f = [&](int i) -> void {
    int payload = state.range(0) / state.range(1);
    int begin = i * payload;
    int end = (i + 1)*payload;
    if (end > state.range(0)) end = state.range(0);
    for (int j = begin; j < end; j++) {
      f(j);
    }
  };
  for (auto _ : state) {
    for (int i = 0; i < state.range(1); i++) {
      wrapped_f(i);
    }
  }
}
BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_std_function_wrapped)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });


BENCHMARK_DEFINE_F(ThreadPoolFixture, BM_openmp)(benchmark::State & state) {
    omp_set_num_threads(state.range(1));
    for (auto _ : state) {
      #pragma omp parallel for 
      for (int i = 0; i < state.range(0); i++) {
        data[i] = 0.1 * i;
      }
    }
}
BENCHMARK_REGISTER_F(ThreadPoolFixture, BM_openmp)
  ->ArgsProduct({
      benchmark::CreateRange(RANGE_LOW, RANGE_HIGH, RANGE_MULTI),
      benchmark::CreateRange(THREAD_LOW, THREAD_HIGH, THREAD_MULTI)
  });


BENCHMARK_MAIN();

