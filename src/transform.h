#pragma once

#include <algorithm>
#include <ranges>
#include <concepts>
#include <utility>
#include <atomic>
#include <vector>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

auto TransformWithProcesses(
        std::ranges::contiguous_range auto &&range,
        std::regular_invocable<std::ranges::range_value_t<decltype(range)>> auto &&func,
        int nprocesses,
        int taskSize = 1
) -> std::ranges::output_range<decltype(func(std::declval<std::ranges::range_value_t<decltype(range)>>()))> auto {
    using OutType = decltype(func(std::declval<std::ranges::range_value_t<decltype(range)>>()));

    auto n = std::ranges::size(range);
    auto taskCount = (n + taskSize - 1) / taskSize;

    struct State {
        std::atomic<size_t> tasksTaken;
    };

    const auto pageSize = getpagesize();
    const auto& align = [pageSize] (auto size) {
        return (size + pageSize - 1) / pageSize * pageSize;
    };

    auto state = static_cast<State*>(mmap(nullptr, align(sizeof(State)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    auto out = static_cast<OutType*>(mmap(nullptr, align(sizeof(OutType) * n), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));

    const auto& proc = [&] {
        while (true) {
            auto task = state->tasksTaken.fetch_add(1);
            if (task >= taskCount) {
                break;
            }

            auto inputBegin = std::ranges::data(range) + task * taskSize;
            auto inputEnd = inputBegin + taskSize;
            auto outputBegin = out + task * taskSize;
            std::transform(inputBegin, inputEnd, outputBegin, func);
        }
    };

    std::vector<pid_t> pids;

    const auto& waitAll = [&] {
        for (auto pid : pids) {
            waitpid(pid, nullptr, 0);
        }

        munmap(state, align(sizeof(State)));
    };

    for (int i = 0; i < nprocesses; ++i) {
        auto pid = fork();
        if (pid < 0) {
            waitAll();
            munmap(out, align(sizeof(OutType) * n));
            throw std::runtime_error{"fork failed"};
        }

        pids.push_back(pid);

        if (pid == 0) {
            proc();
            exit(0);
        }
    }

    waitAll();

    std::vector<OutType> outVec(out, out + n);
    munmap(out, align(sizeof(OutType) * n));
    return outVec;
}
