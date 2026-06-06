#include "KeystoneLogs.hpp"

#include <cerrno>
#include <cstring> // std::strerror
#include <iostream>
#include <ostream>

namespace Keystone {

namespace {

std::ostream& streamFor(Loglevel lv) {
    switch (lv) {
    case Loglevel::FATAL:
    case Loglevel::ERROR:
    case Loglevel::WARN:
        return std::cerr;
    default:
        return std::cout;
    }
}
} // namespace

void Logs::emit(Loglevel msgLevel, std::string_view body, int savedErrno) const {
    std::string line = std::format("{} {}: {}", genPrefix(), levelTag(msgLevel), body);

    if (savedErrno >= 0) {
        line += std::format(" (errno={}: {})", savedErrno, std::strerror(savedErrno));
    }

    streamFor(msgLevel) << line << '\n';
}

} // namespace Keystone
