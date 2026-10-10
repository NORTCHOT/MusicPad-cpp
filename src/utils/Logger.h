#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace musicpad::utils {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void log(LogLevel level, const std::string& message) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_c);
#else
        localtime_r(&now_c, &tm_buf);
#endif

        const char* tag = "INFO";
        const char* color = "\033[32m"; // green
        switch (level) {
            case LogLevel::Debug:
                tag = "DEBUG";
                color = "\033[36m"; // cyan
                break;
            case LogLevel::Info:
                tag = "INFO ";
                color = "\033[32m"; // green
                break;
            case LogLevel::Warn:
                tag = "WARN ";
                color = "\033[33m"; // yellow
                break;
            case LogLevel::Error:
                tag = "ERROR";
                color = "\033[31m"; // red
                break;
        }

        std::cout << "\033[90m["
                  << std::put_time(&tm_buf, "%H:%M:%S") << "."
                  << std::setfill('0') << std::setw(3) << ms.count()
                  << "]\033[0m "
                  << color << "[" << tag << "]\033[0m "
                  << message << std::endl;
    }

    static void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
    static void info(const std::string& msg)  { log(LogLevel::Info, msg); }
    static void warn(const std::string& msg)  { log(LogLevel::Warn, msg); }
    static void error(const std::string& msg) { log(LogLevel::Error, msg); }
};

} // namespace musicpad::utils
