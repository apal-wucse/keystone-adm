#include "UniqueFd.hpp"

UniqueFd::~UniqueFd() noexcept {
    if (fd_ >= 0)
        ::close(fd_);
}

UniqueFd& UniqueFd::operator=(UniqueFd&& o) noexcept {
    if (this != &o) {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = std::exchange(o.fd_, -1);
    }
    return *this;
}
