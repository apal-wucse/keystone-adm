#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Keystone {

enum class Loglevel {
    FATAL = 0,
    ERROR,
    WARN,
    INFO,
    TRACE,
};

class Logs {
public:
    Logs(std::string moduleName) : moduleName(std::move(moduleName)) {}
    Logs(std::string moduleName, std::string subModuleName)
        : moduleName(std::move(moduleName)), subModuleName(std::move(subModuleName)) {}
    Logs(std::string moduleName, Loglevel logLevel)
        : moduleName(std::move(moduleName)), logLevel(logLevel) {}
    Logs(std::string moduleName, std::string subModuleName, Loglevel logLevel)
        : moduleName(std::move(moduleName)), subModuleName(std::move(subModuleName)),
          logLevel(logLevel) {}

    void setModuleName(std::string moduleName_) { moduleName = std::move(moduleName_); }
    void setSubModuleName(std::string moduleName_, std::string subModuleName_) {
        moduleName    = std::move(moduleName_);
        subModuleName = std::move(subModuleName_);
    }
    void setLogLevel(Loglevel logLevel_) { logLevel = logLevel_; }

    template <class... Args> void fatal(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::FATAL, false, fmt, std::forward<Args>(args)...);
    }
    template <class... Args> void error(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::ERROR, false, fmt, std::forward<Args>(args)...);
    }
    template <class... Args> void warn(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::WARN, false, fmt, std::forward<Args>(args)...);
    }
    template <class... Args> void info(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::INFO, false, fmt, std::forward<Args>(args)...);
    }
    template <class... Args> void trace(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::TRACE, false, fmt, std::forward<Args>(args)...);
    }

    template <class... Args> void fatalErrno(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::FATAL, true, fmt, std::forward<Args>(args)...);
    }
    template <class... Args> void errorErrno(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(Loglevel::ERROR, true, fmt, std::forward<Args>(args)...);
    }

private:
    std::string moduleName;
    std::string subModuleName;
    Loglevel logLevel = Loglevel::INFO;

    std::string genPrefix() const {
        std::string prefix = "keystone";
        if (!moduleName.empty()) {
            prefix += "::" + moduleName;
            if (!subModuleName.empty()) {
                prefix += "::" + subModuleName;
            }
        }
        return prefix;
    }

    static std::string_view levelTag(Loglevel lv) noexcept {
        switch (lv) {
        case Loglevel::FATAL:
            return "fatal";
        case Loglevel::ERROR:
            return "error";
        case Loglevel::WARN:
            return "warn";
        case Loglevel::INFO:
            return "info";
        case Loglevel::TRACE:
            return "trace";
        }
        return "unknown";
    }

    void emit(Loglevel msgLevel, std::string_view body, int savedErrno) const;

    template <class... Args>
    void
    logImpl(Loglevel msgLevel, bool withErrno, std::format_string<Args...> fmt, Args&&... args) {
        const int e = withErrno ? errno : -1;

        if (static_cast<int>(msgLevel) > static_cast<int>(logLevel)) {
            return;
        }

        std::string body = std::format(fmt, std::forward<Args>(args)...);
        emit(msgLevel, body, withErrno ? e : -1);
    }
};
} // namespace Keystone
