
#ifndef OPENCMW_MAJORDOMO_DEBUG_H
#define OPENCMW_MAJORDOMO_DEBUG_H

#include <filesystem>
#include <iostream>
#include <mutex>
#if defined(__clang__) // TODO: replace (source_location is part of C++20 but still "experimental" for clang
// does not exist
#else
#include <source_location>
#endif

namespace opencmw::debug {

struct DebugImpl {
    bool _breakLineOnEnd = true;

    DebugImpl() {}

    ~DebugImpl() {
        if (_breakLineOnEnd) {
            operator<<('\n');
        }
    }

    template<typename T>
    DebugImpl &operator<<(T &&val) {
        // static std::mutex print_lock;
        // std::lock_guard   lock{ print_lock };
        std::cerr << std::forward<T>(val);
        return *this;
    }

    DebugImpl(const DebugImpl & /*unused*/) {
    }

    DebugImpl(DebugImpl &&other) noexcept {
        other._breakLineOnEnd = false;
    }
};

// TODO: Make a proper debug function
inline auto
log() {
    return DebugImpl{};
}

#if defined(__clang__)
#define withLocation(para) (log() << __PRETTY_FUNC__ )

#else
inline auto withLocation(const std::source_location location = std::source_location::current()) {
    std::error_code error;
    auto            relative = std::filesystem::relative(location.file_name(), error);
    return log() << (relative.string() /*location.file_name()*/) << ":" << location.line() << " in " << location.function_name() << " --> ";
}
#endif // __clang__

} // namespace opencmw::debug

#endif // include guard
