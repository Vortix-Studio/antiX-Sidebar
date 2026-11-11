#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

class ScopedTimer {
public:
    explicit ScopedTimer(std::string name)
        : name_(std::move(name)), start_(Clock::now()) {}

    ~ScopedTimer() {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - start_).count();

        std::cout << "[DEBUG] " << name_ << ": "
                  << elapsed << " µs (" << elapsed / 1000.0 << " ms)\n";
    }

private:
    using Clock = std::chrono::steady_clock;

    std::string name_;
    Clock::time_point start_;
};

#define PROFILE_SCOPE(name) ScopedTimer timer_##__LINE__(name)
