// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/startup_clock.h"

#include <chrono>

#if defined(__APPLE__)
#include <unistd.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#include <unistd.h>
#endif

namespace fission::services {

std::optional<double> processAgeMilliseconds() {
#if defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(getpid())};
    struct kinfo_proc info{};
    std::size_t size = sizeof info;
    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0 || size == 0) {
        return std::nullopt;
    }
    const struct timeval start = info.kp_proc.p_un.__p_starttime;
    struct timeval now{};
    gettimeofday(&now, nullptr);
    return static_cast<double>(now.tv_sec - start.tv_sec) * 1000.0 +
           static_cast<double>(now.tv_usec - start.tv_usec) / 1000.0;
#elif defined(__linux__)
    // /proc/self/stat: field 22 is the start time in clock ticks after boot; /proc/uptime is
    // the time since boot in seconds. The process name (field 2) may contain spaces, so count
    // the fields after the closing bracket.
    std::ifstream stat("/proc/self/stat");
    std::string line;
    if (!std::getline(stat, line)) {
        return std::nullopt;
    }
    const std::size_t close = line.rfind(')');
    if (close == std::string::npos) {
        return std::nullopt;
    }
    std::size_t position = close + 2; // now at field 3
    for (int field = 3; field < 22; ++field) {
        position = line.find(' ', position);
        if (position == std::string::npos) {
            return std::nullopt;
        }
        ++position;
    }
    const double ticks = std::stod(line.substr(position));
    std::ifstream uptimeFile("/proc/uptime");
    double uptime = 0.0;
    if (!(uptimeFile >> uptime)) {
        return std::nullopt;
    }
    const double perSecond = static_cast<double>(sysconf(_SC_CLK_TCK));
    return (uptime - ticks / perSecond) * 1000.0;
#else
    return std::nullopt;
#endif
}

} // namespace fission::services
