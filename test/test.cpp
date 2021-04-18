#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "transform.h"
#include <numeric>
#include <unordered_set>

TEST_CASE("simple case") {
    std::vector<int> testData(10000);
    std::iota(testData.begin(), testData.end(), 0);
    std::random_shuffle(testData.begin(), testData.end());
    const auto& transformer = [](int value) { return value * 10; };

    std::vector<int> outputData(testData.size());
    std::transform(testData.begin(), testData.end(), outputData.begin(), transformer);
    
    for (int taskSize : {1, 10, 100, 1000, 10000}) {
        for (int processes : {1, 2, 4, 8, 16}) {
            CHECK(TransformWithProcesses(testData, transformer, processes, taskSize) == outputData);
        }
    }
}

TEST_CASE("works with sleep") {
    std::vector<int> testData(100);
    std::iota(testData.begin(), testData.end(), 0);
    std::random_shuffle(testData.begin(), testData.end());
    const auto& transformer = [](int value) { usleep(value * 1000); return value * 10; };

    std::vector<int> outputData(testData.size());
    std::transform(testData.begin(), testData.end(), outputData.begin(), [](int value) { return value * 10; });
    
    for (int taskSize : {1, 10}) {
        for (int processes : {1, 4, 16}) {
            CHECK(TransformWithProcesses(testData, transformer, processes, taskSize) == outputData);
        }
    }
}

TEST_CASE("uses processes") {
    std::vector<int> testData(10000);
    std::iota(testData.begin(), testData.end(), 0);
    std::random_shuffle(testData.begin(), testData.end());
    const auto& transformer = [](int value) { usleep(value); return getpid(); };

    std::unordered_set<int> pids;
    for (int pid : TransformWithProcesses(testData, transformer, 16, 1)) {
        pids.insert(pid);
    }

    CHECK(pids.size() > 1);
}
