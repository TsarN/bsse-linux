#include <iostream>
#include <iomanip>
#include <system_error>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>

void throw_errno(const char* err) {
    throw std::system_error{
        std::make_error_code(std::errc{errno}),
        err
    };
}

struct Fragmentation {
    double avg_run;
    long n_blocks;
};

struct Counter {
    int cur_run = 0;
    int n_runs = 0;
    int last_value = -1;
    int count = 0;

    void next(int value) {
        ++count;

        if (value != 0) {
            if (value == last_value + 1) {
                ++cur_run;
            } else {
                cur_run = 1;
                ++n_runs;
            }

            last_value = value;
        }
    }

    double get() const {
        if (n_runs == 0) return 0.0;
        return static_cast<double>(count) / n_runs;
    }
};

Fragmentation get_file_fragmentation(const char* path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        throw_errno("lstat");
    }

    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_LARGEFILE);
    if (fd == -1) {
        if (errno == ELOOP) {
            std::cerr << "Warn: " << path << " is a symlink, skipping" << std::endl;
            return {
                .avg_run = 0.0,
                .n_blocks = 0
            };
        }

        throw_errno("open");
    }

    int block_size;
    if (ioctl(fd, FIGETBSZ, &block_size) == -1) {
        close(fd);
        throw_errno("ioctl(FIGETBSZ)");
    }

    auto n_blocks = (st.st_size + block_size - 1) / block_size;

    Counter counter;
    for (auto i = 0; i < n_blocks; ++i) {
        int block = i;
        if (ioctl(fd, FIBMAP, &block) == -1) {
            close(fd);
            throw_errno("ioctl(FIBMAP)");
        }
        counter.next(block);
    }

    close(fd);

    return {
        .avg_run = counter.get(),
        .n_blocks = n_blocks
    };
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "Usage: " << argv[0] << " FILE [FILE]..." << std::endl;
        return 1;
    }

    double sum_run = 0.0;
    long sum_blocks = 0;

    for (int i = 1; i < argc; ++i) {
        auto path = argv[i];
        try {
            auto cur = get_file_fragmentation(path);
            sum_run += cur.avg_run;
            sum_blocks += cur.n_blocks;
        } catch (std::exception& e) {
            std::cerr << "=== Error ===" << std::endl;
            std::cerr << e.what() << std::endl;
            std::cerr << "While processing file: " << path << std::endl;
            return 1;
        }
    }
    
    if (sum_blocks == 0) {
        std::cout << "Empty files" << std::endl;
    } else {
        std::cout << "Fragmentation: "
                  << std::setprecision(2) << std::fixed
                  << (sum_run / sum_blocks) * 100.0
                  << "%" << std::endl;
    }

    return 0;
}
