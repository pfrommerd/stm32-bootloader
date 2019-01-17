#include "Bootloader.hpp"
#include "Flash.hpp"

#include <stm32f7xx_hal.h>


#define DEBUG_LEDS

namespace bootloader {
    Mode getMode() {
        RTC_HandleTypeDef rtc_handle;
        rtc_handle.Instance = RTC;
        return HAL_RTCEx_BKUPRead(&rtc_handle, RTC_BKP_DR0) ? Mode::BOOTLOADER : Mode::APP;
    }

    void setMode(Mode m) {
        RTC_HandleTypeDef rtc_handle;
        rtc_handle.Instance = RTC;
        HAL_PWR_EnableBkUpAccess();
        HAL_RTCEx_BKUPWrite(&rtc_handle, RTC_BKP_DR0, m == Mode::BOOTLOADER ? 1 : 0);
        HAL_PWR_DisableBkUpAccess();
    }

    Context::Context(uint8_t* appStart, int boardId,
                        Conn** conns, int numConns) : _boardId(boardId),
                                      _appStart(appStart),
                                      _conns(conns),
                                      _numConns(numConns),
                                      _seqNum(0),
                                      _resetReq(false),
                                      _isWriting(false),
                                      _position(appStart) {}

    void 
    Context::exec(const Msg& cmd, Conn* conn) {
        _history.put(cmd);
        Msg::Type type = cmd.getType();

        Msg result;
        result.setType(Msg::INVALID);
        result.setID(_boardId);
        result.setSeqNum(cmd.getSeqNum());
        result.setLength(4); // Use all 4 bytes

        if (cmd.getType() == Msg::STATUS) {
            result.setType(Msg::ACK);
            result.setData(3, _seqNum);
            (*conn) << result;
            return;
        }

        // Check the sequence number
        if (_seqNum != cmd.getSeqNum()) {
            return;
        }

        switch(type) {
            case Msg::PING:
                // Broadcast our ID so people know we are up
                result.setType(Msg::OKAY);
                result.setData(0, _boardId);
            case Msg::RESET:
                // Set reset flag to true so we reset after handling
                // this request
                _resetReq = true;
                result.setType(Msg::OKAY);
                break;
            case Msg::GET_MODE:
                result.setType(Msg::OKAY);
                result.setData(0, (uint8_t) getMode());
                break;
            case Msg::SET_MODE:
                setMode((Mode) cmd.getData(0));
                if (getMode() == (Mode) cmd.getData(0)) {
                    result.setType(Msg::OKAY);
                    result.setData(0, (uint8_t) getMode());
                } else {
                    result.setType(Msg::ERROR);
                    result.setData(0, (uint8_t) getMode());
                }
                break;
            case Msg::UNLOCK_FLASH:
                _isWriting = true;
                _position = _appStart;
                flash::unlock();

                result.setType(Msg::OKAY);
                break;
            case Msg::LOCK_FLASH:
                _isWriting = false;
                _position = _appStart;
                flash::lock();

                result.setType(Msg::OKAY);
                break;
            case Msg::MOVE:
                _position = (uint8_t*) cmd.getValue();
                result.setType(Msg::OKAY);
                result.setValue((uint32_t) _position);
                break;
            case Msg::MOVE_START:
                _position = _appStart;
                result.setType(Msg::OKAY);
                result.setValue((uint32_t) _position);
                break;
            case Msg::POSITION:
                result.setType(Msg::OKAY);
                result.setValue((uint32_t) _position);
                break;
            case Msg::READ:
                result.setType(Msg::READ);
                result.setValue((*((uint32_t*) _position)));
                _position = _position + 4; // Forward 4 bytes
                break;
            case Msg::WRITE:
                if (_isWriting && _position >= _appStart) {
                    if (flash::write(_position, cmd.getValue())) {
                        _isWriting = false;
                        _position = _appStart;
                        flash::lock();

                        result.setType(Msg::ERROR);
                        result.setData(0, 1);
                    } else {
                        result.setType(Msg::INVALID);
                    }
                } else {
                    _isWriting = false;
                    _position = _appStart;
                    flash::lock();
                    result.setType(Msg::ERROR);
                    result.setData(0, 2);
                }
                _position = _position + 4;
                break;
            case Msg::ERASE:
                if (flash::erase(_appStart, (size_t) cmd.getValue())) {
                    result.setType(Msg::ERROR);
                } else {
                    result.setType(Msg::OKAY);
                }
                break;
            case Msg::INVALID:
            default:
                result.setType(Msg::INVALID);
        }

        // Send back the result
        if (result.getType() != Msg::INVALID)
            (*conn) << result;

        // Increment the sequence number (with 255 rollover definitely right)
        _seqNum = (uint8_t) (((uint16_t) _seqNum + 1) % 256);
    }

    void
    Context::run() {
        #ifdef DEBUG_LEDS
        // Debug LED 1
        GPIO_InitTypeDef pin;
        pin.Pin = GPIO_PIN_0;
        pin.Mode = GPIO_MODE_OUTPUT_PP;
        pin.Alternate = 0;
        pin.Pull = GPIO_NOPULL;
        pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOB, &pin);
        // Debug LED 2
        pin.Pin = GPIO_PIN_7;
        pin.Mode = GPIO_MODE_OUTPUT_PP;
        pin.Alternate = 0;
        pin.Pull = GPIO_NOPULL;
        pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOB, &pin);
        // Debug LED 3
        pin.Pin = GPIO_PIN_14;
        pin.Mode = GPIO_MODE_OUTPUT_PP;
        pin.Alternate = 0;
        pin.Pull = GPIO_NOPULL;
        pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOB, &pin);
        #endif

        Msg msg;
        Conn* src = nullptr; // Conn msg came from
        while (!_resetReq) {
            if (_numConns <= 0) asm("bkpt 255"); // No connections! reset
            // Check to see if any of the connections
            // are ready to read
            for (int i = 0; i < _numConns; i++) {
                Conn* c = _conns[i];
                if (c->hasData()) {
                    (*c) >> msg;
                    // If there was an
                    // error reading, just go on
                    if (msg.hasError()) {
                        // Toggle red led for error
                        #ifdef DEBUG_LEDS
                        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
                        #endif
                        continue;
                    }
                    src = c;
                    break;
                }
            }
            if (!src) continue; // Try to read again

            // Toggle green led when reading
            #ifdef DEBUG_LEDS
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
            #endif

            // Check if we should handle this message
            bool handle = msg.getID() == _boardId || msg.hasError() ||
                          msg.getType() == Msg::PING;
            bool forward = !handle || msg.getType() == Msg::PING;

            if (forward) {
                // Transmit result/forward msg
                for (int i = 0; i < _numConns; i++) {
                    Conn* c = _conns[i];
                    if (c == src) continue;
                    (*c) << msg;
                }
            }

            if (handle) {
                // Toggle blue LED when processing message
                #ifdef DEBUG_LEDS
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7);
                #endif
                exec(msg, src);
            }
            src = nullptr;
        }
        // Flush the connections
        // before we reset
        for (int i = 0; i < _numConns; i++) {
            Conn* c = _conns[i];
            c->flush();
        }
        NVIC_SystemReset();
    }
}

