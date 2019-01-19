#pragma once

#include "Bootloader.hpp"
#include "Pin.hpp"

namespace bootloader {
    namespace can {
        class Can : public Conn {
        public:
            Can();
            Can(const Pin& rx, const Pin& tx, int baud);
            virtual ~Can();

            void close() override;

            bool isOpen() const override;

            bool hasData() const override;

            size_t getReadWindow() const override;
            size_t getWriteWindow() const override;

            void flush() override;

            Conn& operator<<(const Msg& w) override; // Write
            Conn& operator>>(Msg& r) override; // Read
        private:
            int _idx;
        };
    }
}

