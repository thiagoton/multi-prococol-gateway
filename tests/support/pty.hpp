#pragma once

#include "support/fd.hpp"

#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

namespace support {

class PtyPair {
public:
    PtyPair()
    {
        master_.reset(::posix_openpt(O_RDWR | O_NOCTTY));
        if (!master_)
            throw std::runtime_error("posix_openpt() failed");

        if (::grantpt(master_.get()) != 0 || ::unlockpt(master_.get()) != 0)
            throw std::runtime_error("grantpt()/unlockpt() failed");

        const char* name = ::ptsname(master_.get());
        if (name == nullptr)
            throw std::runtime_error("ptsname() failed");
        slavePath_ = name;

        struct termios tty;
        if (::tcgetattr(master_.get(), &tty) != 0)
            throw std::runtime_error("tcgetattr() failed");
        ::cfmakeraw(&tty);
        if (::tcsetattr(master_.get(), TCSANOW, &tty) != 0)
            throw std::runtime_error("tcsetattr() failed");
    }

    int master() const
    {
        return master_.get();
    }

    const std::string& slavePath() const
    {
        return slavePath_;
    }

    void closeMaster()
    {
        master_.reset();
    }

private:
    FileDescriptor master_;
    std::string slavePath_;
};

}
