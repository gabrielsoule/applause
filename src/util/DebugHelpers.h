#pragma once

#include <format>
#include <iostream>
#include <chrono>
#include <string_view>
#include <cassert>

#ifndef NDEBUG

namespace debug {
    
    enum class Level {
        DBG = 0,
        INFO = 1,
        WARN = 2,
        ERR = 3
    };
    
    constexpr std::string_view level_name(const Level level) {
        switch (level) {
            case Level::DBG:   return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERR:   return "ERROR";
        }
        return "UNKNOWN";
    }
    
    inline std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm* tm = std::localtime(&time);
        return std::format("{:02d}:{:02d}:{:02d}.{:03d}", 
                          tm->tm_hour, tm->tm_min, tm->tm_sec, ms.count());
    }
    
    template<typename... Args>
    void log(Level level, std::string_view file, int line, std::string_view func,
             std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        auto filename = file.substr(file.find_last_of("/\\") + 1);
        
        std::cout << std::format("[{}] {} {}:{} ({}) {}",
                                timestamp(),
                                level_name(level),
                                filename,
                                line,
                                func,
                                msg) << std::endl;
    }
    
    template<typename T>
    std::string var_string(std::string_view name, const T& value) {
        return std::format("[{}={}]", name, value);
    }
    
}

#define LOG_TRACE(...) debug::log(debug::Level::TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DBG(...) debug::log(debug::Level::DBG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  debug::log(debug::Level::INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  debug::log(debug::Level::WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERR(...) debug::log(debug::Level::ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_VAR(x) debug::var_string(#x, x)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            LOG_ERR("Assertion failed: {} ({})", message, #condition); \
            assert(false); \
        } \
    } while(0)

#define ASSERT_FALSE(message) \
    do { \
        LOG_ERR("Assertion failed: {}", message); \
        assert(false); \
    } while(0)

#else

#define LOG_TRACE(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define LOG_INFO(...)  ((void)0)
#define LOG_WARN(...)  ((void)0)
#define LOG_ERR(...) ((void)0)
#define DBG(...) ((void)0)
#define LOGVAR(x) ""
#define ASSERT(condition, message) ((void)0)
#define ASSERT_FALSE(message) ((void)0)

#endif
