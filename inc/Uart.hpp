#pragma once

#include "Bootloader.hpp"
#include "Pin.hpp"

namespace bootloader {
    namespace uart {
        class Uart : public Conn {
        public:
            Uart();
            Uart(const Pin& rx, const Pin& tx, int baud);
            virtual ~Uart();

            bool isOpen() const override;
            void close() override;

            bool hasData() const override;

            size_t getReadWindow() const override;
            size_t getWriteWindow() const override;

            void flush();

            Conn& operator<<(const Msg& w) override; // Write
            Conn& operator>>(Msg& r) override; // Read
        private:
            int _idx;
        };
    }
}

