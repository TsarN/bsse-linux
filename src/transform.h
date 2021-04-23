#pragma once

#include <algorithm>
#include <ranges>
#include <concepts>
#include <utility>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

auto TransformWithProcesses(
        std::ranges::contiguous_range auto &&range,
        std::regular_invocable<std::ranges::range_value_t<decltype(range)>> auto &&func,
        int nprocesses,
        int taskSize = 1,
        bool balance = true
) -> std::ranges::output_range<decltype(func(std::declval<std::ranges::range_value_t<decltype(range)>>()))> auto {
    using OutType = decltype(func(std::declval<std::ranges::range_value_t<decltype(range)>>()));

    auto n = std::ranges::size(range);
    auto taskCount = (n + taskSize - 1) / taskSize;

    struct State {
        std::atomic<size_t> tasksTaken = 0;
    };

    static_assert(std::atomic<size_t>::is_always_lock_free);

    const auto pageSize = getpagesize();
    const auto& align = [pageSize] (auto size) {
        return (size + pageSize - 1) / pageSize * pageSize;
    };

    auto state = std::unique_ptr<State, std::function<void(State*)>>{
        static_cast<State*>(mmap(nullptr, align(sizeof(State)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)),
        [&](State* ptr) { munmap(ptr, align(sizeof(State))); }
    };
    if (!state) {
        throw std::runtime_error{"mmap failed"};
    }
    new (state.get()) State{};

    auto out = std::unique_ptr<OutType[], std::function<void(OutType*)>>{
        static_cast<OutType*>(mmap(nullptr, align(sizeof(OutType) * n), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)),
        [&](OutType* ptr) { munmap(ptr, align(sizeof(OutType) * n)); }
    };
    if (!out) {
        throw std::runtime_error{"mmap failed"};
    }

    const auto& proc = [&](auto takeTask) {
        while (true) {
            auto task = takeTask();
            if (task == -1) {
                break;
            }

            auto inputBegin = std::ranges::data(range) + task * taskSize;
            auto inputEnd = inputBegin + ((task == taskCount - 1 && n % taskSize > 0) ? (n % taskSize) : taskSize);
            auto outputBegin = out.get() + task * taskSize;
            std::transform(inputBegin, inputEnd, outputBegin, func);
        }
    };

    std::vector<pid_t> pids;

    const auto& waitAll = [&] {
        for (auto pid : pids) {
            waitpid(pid, nullptr, 0);
        }
    };

    for (int i = 0; i < nprocesses; ++i) {
        auto pid = fork();
        if (pid < 0) {
            waitAll();
            throw std::runtime_error{"fork failed"};
        }

        pids.push_back(pid);

        if (pid == 0) {
            if (balance) {
                proc([&]() -> int {
                    auto task = state->tasksTaken.fetch_add(1);
                    return (task >= taskCount) ? -1 : task;
                }); 
            } else {
                const auto perWorker = (taskCount + nprocesses - 1) / nprocesses;
                proc([&, perWorker, tasksTaken = perWorker * i]() mutable -> int {
                    auto task = tasksTaken++;
                    if (task >= taskCount || task >= perWorker * (i + 1)) {
                        return -1;
                    }
                    return task;
                });
            }
            exit(0);
        }
    }

    waitAll();

    std::vector<OutType> outVec(out.get(), out.get() + n);
    return outVec;
}
