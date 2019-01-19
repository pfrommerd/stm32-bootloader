#define BOARD_ID 1

#define CONF\
    Uart uart(PD9, PD8, 921600);\
    Can can(PD0, PD1, 500000);\
    Conn* conns[] = {&uart, &can};

#include "../src/main.cpp"

