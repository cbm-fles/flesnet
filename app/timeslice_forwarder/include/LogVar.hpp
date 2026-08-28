#pragma once

#include <chrono>
#include <memory>

template <typename T>
class Avg {
private:
    std::chrono::duration<uint64_t, std::milli> t_last_get_{};

public:
    std::shared_ptr<T> value = nullptr;
    Avg() {
        t_last_get_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        value = std::make_shared<T>();
    };

    std::shared_ptr<T> per_seconds() {
        std::shared_ptr<T> val = std::make_shared<T>();
        auto t_now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        auto t_diff = (t_now - t_last_get_).count();
        if (t_diff > 0) {
            *val = (T(*(std::reinterpret_pointer_cast<T>(this->value)) * 1000) / T(t_diff));
        } else {
            val->store((std::reinterpret_pointer_cast<T>(this->value))->load());
        }
        *(std::reinterpret_pointer_cast<T>(this->value)) = 0;
        t_last_get_ = t_now;
        return val;
    }
};