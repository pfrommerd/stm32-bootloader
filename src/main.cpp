#include "Bootloader.hpp"
#include "System.hpp"
#include "Uart.hpp"
#include "Can.hpp"
#include "Pin.hpp"

using namespace bootloader;
using namespace bootloader::pins;
using namespace bootloader::uart;
using namespace bootloader::can;

// Defaults
#ifndef BOARD_ID
#define BOARD_ID 0
#endif

#ifndef CONF
#define CONF Conn* conns[] = {};
#endif

#ifndef APP_START
#define APP_START 0x08080000
#endif

void start_bootloader() {

    // The config
    CONF;

    // Board ID 1, 1 connection
    Context ctx((uint8_t*) APP_START, BOARD_ID, conns, sizeof(conns)/sizeof(Conn*));
    ctx.run();
}

int main(void) {
    bootloader::system::partial_init();

    // Powers up main clocks an everything we need
    Mode mode = getMode();
    if (mode == Mode::BOOTLOADER) {
        bootloader::system::full_init();
        start_bootloader();
    } else {
        setMode(Mode::BOOTLOADER);
        bootloader::system::run((uint32_t*) APP_START);
    }
}

