namespace bootloader {
    namespace system {
        // Brings up hal, rtc backup registers
        void partial_init();
        // brings up all the clocks and things
        // partial_init should be called first
        void full_init();
        void deinit();
        void run(void* app);
        inline void breakpoint() { asm("bkpt 255"); }
    }
}
