#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <span>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <cxxopts.hpp>

enum class Mode {
    iostream,
    syscalls,
    mmap
};

struct Settings {
    long blockSize;
    Mode mode;
    std::filesystem::path path;
};

std::vector<long> counter(256);

void runBenchmark(const Settings& settings, auto&& doRead) {
    std::vector<char> buf(settings.blockSize);

    while (doRead(buf)) {
        for (auto byte : buf) {
            ++counter[(unsigned char)byte];
        }
    }
}

void runBenchmarkIostream(Settings settings) {
    std::ifstream file{settings.path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{"File open failed"};
    }

    if (settings.blockSize == 0) {
        settings.blockSize = std::filesystem::file_size(settings.path);
    }

    runBenchmark(settings, [&](auto& buf) {
        return (bool)file.read(buf.data(), settings.blockSize);
    });
}

void runBenchmarkSyscalls(Settings settings) {
    int fd = open(settings.path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error{"File open failed"};
    }

    if (settings.blockSize == 0) {
        struct stat st;
        fstat(fd, &st);
        settings.blockSize = st.st_size;
    }

    runBenchmark(settings, [&](auto& buf) {
        return read(fd, buf.data(), settings.blockSize) > 0;
    });

    close(fd);
}

void runBenchmarkMmap(Settings settings) {
    int fd = open(settings.path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error{"File open failed"};
    }

    struct stat st;
    fstat(fd, &st);

    std::size_t size = st.st_size;

    auto ptr = (std::uint8_t*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        throw std::runtime_error{"mmap failed"};
    }

    for (auto byte : std::span{ptr, size}) {
        ++counter[byte];
    }

    munmap(ptr, size);
    close(fd);
}

int main(int argc, char **argv) {
    cxxopts::Options options{"file_read_benchmark", "Benchmark file read speed"};

    options.add_options()
        ("b,block-size", "Block size, 0 = read the entire file in one go", cxxopts::value<long>()->default_value("0"))
        ("m,mode", "Mode: iostream, syscalls, mmap", cxxopts::value<std::string>()->default_value("iostream"))
    ;
    
    cxxopts::ParseResult result;
    Settings settings;

    try {
        result = options.parse(argc, argv);

        settings = Settings{
            .blockSize = result["block-size"].as<long>(),
            .mode = [&]{
                if (result["mode"].as<std::string>() == "iostream") {
                    return Mode::iostream;
                } else if (result["mode"].as<std::string>() == "syscalls") {
                    return Mode::syscalls;
                } else if (result["mode"].as<std::string>() == "mmap") {
                    return Mode::mmap;
                } else {
                    throw std::runtime_error{"Invalid value for 'mode'"};
                }
            }(),
            .path = ""
        };


    } catch (cxxopts::OptionException& e) {
        std::cout << e.what() << std::endl;
        std::cout << options.help();
        return 1;
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    for (const auto& path : result.unmatched()) {
        settings.path = path;

        if (settings.mode == Mode::iostream) {
            runBenchmarkIostream(settings);
        } else if (settings.mode == Mode::syscalls) {
            runBenchmarkSyscalls(settings);
        } else if (settings.mode == Mode::mmap) {
            runBenchmarkMmap(settings);
        }
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < counter.size(); ++i) {
        std::cout << "byte " << i << " : " << counter[i] << "\n";
    }

    std::cout << "=====\n";
    std::chrono::duration<double, std::milli> duration = t2 - t1;
    std::cout << "Real time: " << duration.count() << " ms\n";

    return 0;
}
