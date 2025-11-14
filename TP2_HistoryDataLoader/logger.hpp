#pragma once
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>

class Logger {
public:
    enum Level { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3 };

    explicit Logger(const std::string& file, Level lvl)
        : level_(lvl), out_(file, std::ios::app)
    {}

    void log(Level lvl, const std::string& msg) {
        if (lvl > level_) return;
        std::lock_guard<std::mutex> lk(m_);
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        out_ << "[" << std::put_time(&tm, "%F %T") << "] "
            << level_name(lvl) << " " << msg << "\n";
        out_.flush();
    }

    void error(const std::string& m){ log(ERROR, m); }
    void warn(const std::string& m){ log(WARN, m); }
    void info(const std::string& m){ log(INFO, m); }
    void debug(const std::string& m){ log(DEBUG, m); }

private:
    std::string level_name(Level l) {
        switch (l) {
            case ERROR: return "ERR";
            case WARN:  return "WRN";
            case INFO:  return "INF";
            case DEBUG: return "DBG";
        }
        return "UNK";
    }

    Level level_;
    std::ofstream out_;
    std::mutex m_;
};
