#pragma once
#include <stm32f7xx_hal.h>

namespace bootloader {
    namespace pins {
#if defined(STM32F7)
        constexpr GPIO_TypeDef* const ports[11] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, 
                                                    GPIOF, GPIOG, GPIOH, GPIOI, GPIOJ, 
                                                    GPIOK };
#endif
    }
    class Pin {
    public:
        // enums
        enum class Pull : uint32_t {
            NONE = GPIO_NOPULL,
            UP = GPIO_PULLUP,
            DOWN = GPIO_PULLDOWN
        };
        enum class Mode : uint32_t {
            INPUT           = GPIO_MODE_INPUT,
            PUSH_PULL       = GPIO_MODE_OUTPUT_PP,
            OPEN_DRAIN      = GPIO_MODE_OUTPUT_OD,
            ALT_PUSH_PULL   = GPIO_MODE_AF_PP,
            ALT_OPEN_DRAIN  = GPIO_MODE_AF_OD,

            ANALOG          = GPIO_MODE_ANALOG,

            RISE_INTERRUPT  = GPIO_MODE_IT_RISING,
            FALL_INTERRUPT  = GPIO_MODE_IT_FALLING,
            CHANGE_INTERRUPT= GPIO_MODE_IT_RISING_FALLING,

            RISE_EVENT      = GPIO_MODE_EVT_RISING,
            FALL_EVENT      = GPIO_MODE_EVT_FALLING,
            CHANGE_EVENT    = GPIO_MODE_EVT_RISING_FALLING,
        };
        enum class Speed : uint32_t {
            LOW = GPIO_SPEED_FREQ_LOW,
            MEDIUM = GPIO_SPEED_FREQ_MEDIUM,
            HIGH = GPIO_SPEED_FREQ_HIGH,
            VERY_HIGH = GPIO_SPEED_FREQ_VERY_HIGH
        };

        constexpr Pin() : _port(0xF), _number(0) {}
        constexpr Pin(char p, uint8_t n) : _port(isValid(p - 'A', n) ? (p -'A') : 0xF),
                                          _number(isValid(p - 'A', n) ? n : 0) {}

        constexpr GPIO_TypeDef* port() const {
            return isValid() ? pins::ports[_port] : nullptr;
        }

        constexpr int pin() const {
            return 1 << _number;
        }

        constexpr int number() const {
            return isValid() ? _number : -1;
        }

        // Configuration stuff
        inline void init(Mode mode, Pull pull = Pull::NONE, Speed speed = Speed::VERY_HIGH,
                             uint8_t function = 0) {
            if (!isValid()) return;
            GPIO_InitTypeDef gpio;
            gpio.Pin = pin();
            gpio.Mode = static_cast<uint32_t>(mode);
            gpio.Pull = static_cast<uint32_t>(pull);
            gpio.Speed = static_cast<uint32_t>(speed);
            gpio.Alternate = function;
            HAL_GPIO_Init(port(), &gpio);
        }

        inline void deinit() {
            if (!isValid()) return;
            HAL_GPIO_DeInit(port(), pin());
        }

        // Operators and other utilities

        constexpr bool operator==(const Pin& other) const {
            return other._port == _port && other._number == _number;
        }
        constexpr bool operator!=(const Pin& other) const {
            return !(*this == other);
        }
                
        constexpr bool isValid() const {
            return isValid(_port, _number);
        }

        constexpr static bool isValid(const uint8_t port, const uint8_t pin) {
            constexpr size_t maxPort = sizeof(pins::ports) / sizeof(pins::ports[0]);
            return !(port >= maxPort || (port == maxPort && pin >= 8));
        }
    private:
        uint8_t _port : 4;
        uint8_t _number : 4;

    };
    namespace pins {
        constexpr Pin NC;
#if defined(STM32F7)
        // A pins
        constexpr Pin PA0('A', 0);
        constexpr Pin PA1('A', 1);
        constexpr Pin PA2('A', 2);
        constexpr Pin PA3('A', 3); 
        constexpr Pin PA4('A', 4);
        constexpr Pin PA5('A', 5);
        constexpr Pin PA6('A', 6);
        constexpr Pin PA7('A', 7); 
        constexpr Pin PA8('A', 8);
        constexpr Pin PA9('A', 9);
        constexpr Pin PA10('A', 10);
        constexpr Pin PA11('A', 11); 
        constexpr Pin PA12('A', 12); 
        constexpr Pin PA13('A', 13); 
        constexpr Pin PA14('A', 14); 
        constexpr Pin PA15('A', 15); 

        // B pins
        constexpr Pin PB0('B', 0);
        constexpr Pin PB1('B', 1);
        constexpr Pin PB2('B', 2);
        constexpr Pin PB3('B', 3); 
        constexpr Pin PB4('B', 4);
        constexpr Pin PB5('B', 5);
        constexpr Pin PB6('B', 6);
        constexpr Pin PB7('B', 7); 
        constexpr Pin PB8('B', 8);
        constexpr Pin PB9('B', 9);
        constexpr Pin PB10('B', 10);
        constexpr Pin PB11('B', 11); 
        constexpr Pin PB12('B', 12); 
        constexpr Pin PB13('B', 13); 
        constexpr Pin PB14('B', 14); 
        constexpr Pin PB15('B', 15); 

        // C pins
        constexpr Pin PC0('C', 0);
        constexpr Pin PC1('C', 1);
        constexpr Pin PC2('C', 2);
        constexpr Pin PC3('C', 3); 
        constexpr Pin PC4('C', 4);
        constexpr Pin PC5('C', 5);
        constexpr Pin PC6('C', 6);
        constexpr Pin PC7('C', 7); 
        constexpr Pin PC8('C', 8);
        constexpr Pin PC9('C', 9);
        constexpr Pin PC10('C', 10);
        constexpr Pin PC11('C', 11); 
        constexpr Pin PC12('C', 12); 
        constexpr Pin PC13('C', 13); 
        constexpr Pin PC14('C', 14); 
        constexpr Pin PC15('C', 15); 

        // D pins
        constexpr Pin PD0('D', 0);
        constexpr Pin PD1('D', 1);
        constexpr Pin PD2('D', 2);
        constexpr Pin PD3('D', 3); 
        constexpr Pin PD4('D', 4);
        constexpr Pin PD5('D', 5);
        constexpr Pin PD6('D', 6);
        constexpr Pin PD7('D', 7); 
        constexpr Pin PD8('D', 8);
        constexpr Pin PD9('D', 9);
        constexpr Pin PD10('D', 10);
        constexpr Pin PD11('D', 11); 
        constexpr Pin PD12('D', 12); 
        constexpr Pin PD13('D', 13); 
        constexpr Pin PD14('D', 14); 
        constexpr Pin PD15('D', 15); 

        // H pins
        constexpr Pin PH0('H', 0);
        constexpr Pin PH1('H', 1);
        constexpr Pin PH2('H', 2);
        constexpr Pin PH3('H', 3); 
        constexpr Pin PH4('H', 4);
        constexpr Pin PH5('H', 5);
        constexpr Pin PH6('H', 6);
        constexpr Pin PH7('H', 7); 
        constexpr Pin PH8('H', 8);
        constexpr Pin PH9('H', 9);
        constexpr Pin PH10('H', 10);
        constexpr Pin PH11('H', 11); 
        constexpr Pin PH12('H', 12); 
        constexpr Pin PH13('H', 13); 
        constexpr Pin PH14('H', 14); 
        constexpr Pin PH15('H', 15); 

        // I pins
        constexpr Pin PI0('I', 0);
        constexpr Pin PI1('I', 1);
        constexpr Pin PI2('I', 2);
        constexpr Pin PI3('I', 3); 
        constexpr Pin PI4('I', 4);
        constexpr Pin PI5('I', 5);
        constexpr Pin PI6('I', 6);
        constexpr Pin PI7('I', 7); 
        constexpr Pin PI8('I', 8);
        constexpr Pin PI9('I', 9);
        constexpr Pin PI10('I', 10);
        constexpr Pin PI11('I', 11); 
        constexpr Pin PI12('I', 12); 
        constexpr Pin PI13('I', 13); 
        constexpr Pin PI14('I', 14); 
        constexpr Pin PI15('I', 15); 
#endif
    }
}
