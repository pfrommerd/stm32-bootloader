#pragma once

#include <cstddef>
#include <cinttypes>

namespace bootloader {
    namespace flash {
        void unlock();
        void lock();

        int write(uint8_t* ptr, uint32_t data);
        int erase(uint8_t* start, size_t length);
    }
}
