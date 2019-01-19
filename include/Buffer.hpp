#pragma once

#include <cstring>

namespace bootloader {
    template<typename T, int cap>
    class Buffer {
    public:
        Buffer() : _buf(), _idx(0), _len(0) {}

        inline bool empty() const {
            return _len == 0;
        }

        inline bool full() const {
            return _len == cap;
        }
        
        inline size_t size() const {
            return _len;
        }

        inline size_t free() const {
            return cap - _len;
        }

        // Returns first element
        const T& front() const {
            return _buf[_idx];
        }

        const T& pop() {
            if (empty()) asm("bkpt 255"); // ERROR!
            size_t i = _idx;
            _idx = (_idx + 1) % cap;
            _len = _len - 1;

            return _buf[i];
        }

        bool push(const T& v) {
            if (full()) return false;
            _buf[(_idx + _len) % cap] = v;
            _len = _len + 1;
            return true;
        }

        // Just dumps into the index
        // to be used for temporary
        // storage style stuff
        // puts things in the front
        void put(const T& v) {
            _idx = _idx == 0 ? cap - 1 : _idx - 1;
            _buf[_idx] = v;
            if (_len < cap) _len = _len + 1;
        }
        // like put, but for the back
        void add(const T& v) {
            _buf[(_idx + _len) % cap] = v;
            if (_len < cap) _len = _len + 1;
            else _idx = (_idx + 1) % cap; // we're overriding the front
        }

        void clear() {
            _idx = 0;
            _len = 0;
            memset(_buf, 0, sizeof(_buf));
        }

    private:
        T _buf[cap];
        size_t _idx; // Index of next element to return
        size_t _len; // Number of elements in the buffer
    };
}
