#include "Uart.hpp"
#include "Buffer.hpp"

#include <stm32f7xx_hal.h>
#include <string.h>

#define USE_CHECKSUM

namespace bootloader{
    namespace uart {
        class UartDriver {
        public:
            UartDriver(USART_TypeDef* uart) : _open(false),
                           _rxPin(),
                           _txPin(),
                           _handle(UART_HandleTypeDef()),
                           _rxBuffer(),
                           _txBuffer(),
                           _transmitting(false),
                           _error(false) {
                _handle.Instance = uart;
            }
            UART_HandleTypeDef* getHandle() { return &_handle; }

            void open(const Pin& rx, const Pin& tx, int baud) {
                _rxPin = rx;
                _txPin = tx;

                _handle.Init.BaudRate   = baud;
                _handle.Init.WordLength = UART_WORDLENGTH_8B;
                _handle.Init.StopBits   = UART_STOPBITS_1;
                _handle.Init.Parity     = UART_PARITY_NONE;
                _handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
                _handle.Init.Mode       = UART_MODE_TX_RX;

                if (HAL_UART_Init(&_handle) != HAL_OK) asm("bkpt 255");
                __HAL_UART_ENABLE_IT(&_handle, UART_IT_RXNE | UART_IT_TC);

                _open = true;
            }

            void close() {
            }

            bool isOpen() {
                return _open;
            }

            void _mspInit() {
                __USART3_CLK_ENABLE();

                _txPin.init(Pin::Mode::ALT_PUSH_PULL, Pin::Pull::NONE, Pin::Speed::VERY_HIGH,
                                GPIO_AF7_USART3);
                _rxPin.init(Pin::Mode::ALT_OPEN_DRAIN, Pin::Pull::NONE, Pin::Speed::VERY_HIGH,
                                GPIO_AF7_USART3);

                _irqEnable();

                // Start read
                SET_BIT(_handle.Instance->CR3, USART_CR3_EIE);
                SET_BIT(_handle.Instance->CR1, USART_CR1_PEIE | USART_CR1_RXNEIE);
            }

            void _irqEnable() {
                if (_handle.Instance == USART3) {
                    HAL_NVIC_SetPriority(USART3_IRQn,0,0);
                    HAL_NVIC_EnableIRQ(USART3_IRQn);
                }
            }
            void _irqDisable() {
                if (_handle.Instance == USART3) {
                    HAL_NVIC_DisableIRQ(USART3_IRQn);
                }
            }
            void _mspDeInit() {
                __USART3_CLK_DISABLE();
            }

            void _irq() {
                uint32_t isrflags   = READ_REG(_handle.Instance->ISR);
                uint32_t cr1its     = READ_REG(_handle.Instance->CR1);
                uint32_t cr3its     = READ_REG(_handle.Instance->CR3);
                uint32_t errorflags = (isrflags & 
                        (uint32_t)(USART_ISR_PE | USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE));

                if (errorflags == RESET) {
                    if (((isrflags & USART_ISR_RXNE) != RESET)
                             && ((cr1its & USART_CR1_RXNEIE) != RESET)) {
                        // We have received something
                        _rxIRQ();
                        return;
                    }
                }

                if((errorflags != RESET)
                     && (((cr3its & USART_CR3_EIE) != RESET)
                      || ((cr1its & (USART_CR1_RXNEIE | USART_CR1_PEIE)) != RESET))) {

                    if (((isrflags & USART_ISR_PE) != RESET) && ((cr1its & USART_CR1_PEIE) != RESET)) {
                        __HAL_UART_CLEAR_IT(&_handle, UART_CLEAR_PEF);
                    }
                    if (((isrflags & USART_ISR_FE) != RESET) && ((cr3its & USART_CR3_EIE) != RESET)) {
                        __HAL_UART_CLEAR_IT(&_handle, UART_CLEAR_FEF);
                    }
                    if (((isrflags & USART_ISR_NE) != RESET) && ((cr3its & USART_CR3_EIE) != RESET)) {
                        __HAL_UART_CLEAR_IT(&_handle, UART_CLEAR_NEF);
                    }
                    if(((isrflags & USART_ISR_ORE) != RESET) &&
                           (((cr1its & USART_CR1_RXNEIE) != RESET) || ((cr3its & USART_CR3_EIE) != RESET))) {
                        __HAL_UART_CLEAR_IT(&_handle, UART_CLEAR_OREF);
                    }

                    // Stop reading, clear buffers
                    resetReading();
                    // Set the error flag
                    _error = true;

                    if (((isrflags & USART_ISR_RXNE) != RESET)
                             && ((cr1its & USART_CR1_RXNEIE) != RESET)) {
                        // We have received something
                        _rxIRQ();
                    }
					return;
                }
                if (((isrflags & USART_ISR_TXE) != RESET)
                        && ((cr1its & USART_CR1_TXEIE) != RESET)) {
                    _txIRQ();
                    return;
                }
                if(((isrflags & USART_ISR_TC) != RESET)
                        && ((cr1its & USART_CR1_TCIE) != RESET)) {
                    _txCpltIRQ();
                    return;
                }
            }

            void _txIRQ() {
                if (_txBuffer.empty()) {
                    // Unset the transmit bit
                    CLEAR_BIT(_handle.Instance->CR1, USART_CR1_TXEIE);
                    // Set transmission complete
                    SET_BIT(_handle.Instance->CR1, USART_CR1_TCIE);
                } else {
                    uint8_t val = _txBuffer.pop();
                    // Set transmission register
                    _handle.Instance->TDR = val;
                }
            }

            void _txCpltIRQ() {
                // Transmission complete! Unset transmission complete and transmit
                // handles
                CLEAR_BIT(_handle.Instance->CR1, (USART_CR1_TXEIE | USART_CR1_TCIE));
                _transmitting = false;
            }

            void _rxIRQ() {
                uint8_t data = (_handle.Instance->RDR);
                if (!_rxBuffer.push(data)) {
                    // If full
                    _error = true;
                    resetReading();
                }
            }

            void _transmit() {
                if (!_transmitting) {
                    _transmitting = true;
                    // Set the send bit, will cause an interrupt to do the actual sending
                    // set transmit data register interrupt
                    SET_BIT(_handle.Instance->CR1, USART_CR1_TXEIE); 
                }
            }

            bool hasData() const {
                return !_rxBuffer.empty() || _error;
            }

            void write(uint8_t* msg, size_t len) {
                // Wait until there is enough space
                while (_txBuffer.free() < len) {
                    if (!_transmitting) _transmit();
                }
                // Copy to buffer
                for (size_t i = 0; i < len; i++) _txBuffer.push(msg[i]);
                // Make sure the transmit bit is set
                // Won't do anything if it isn't
                if (!_transmitting) _transmit();
            }

            void resetReading() {
                __HAL_UART_SEND_REQ(&_handle, UART_RXDATA_FLUSH_REQUEST);
                _rxBuffer.clear();
                // Error, cancel the read to clear things up
                CLEAR_BIT(_handle.Instance->CR1, (USART_CR1_RXNEIE | USART_CR1_PEIE));
                CLEAR_BIT(_handle.Instance->CR3, USART_CR3_EIE);
            }

            void flush() {
                while (_transmitting) {}
            }

            int read(uint8_t* msg, size_t len) {
                size_t remaining = len;
                while (remaining > 0) {
                    uint32_t tickstart = HAL_GetTick(); // For timeout
                    while (!hasData()) {
                        if (remaining < len &&
                            HAL_GetTick() - tickstart >= 10) {
                            _error = true; // Timeout
                        }
                    }
                    if (_error) {
                        _error = false;
                        // Restart reading if need be
                        SET_BIT(_handle.Instance->CR3, USART_CR3_EIE);
                        SET_BIT(_handle.Instance->CR1, USART_CR1_PEIE | USART_CR1_RXNEIE);
                        return -1;
                    }
                    msg[len - remaining] = _rxBuffer.pop();
                    remaining--;
                }
                return 0;
            }

            size_t getReadWindow() const {
                return _rxBuffer.free();
            }

            size_t getWriteWindow() const {
                return _txBuffer.free();
            }

            bool hadPartialRead() const {
                return _hadPartialRead;
            }
            void setPartialRead(bool partial) {
                _hadPartialRead = partial;
            }

        private:
            bool _open;
            Pin  _rxPin;
            Pin  _txPin;
            UART_HandleTypeDef    _handle;
            Buffer<uint8_t, 8192> _rxBuffer;
            Buffer<uint8_t, 8192> _txBuffer;
            bool _transmitting;
            bool _error;
            bool _hadPartialRead;
        };

        static UartDriver s_drivers[3] = { UartDriver(USART1), UartDriver(USART2), UartDriver(USART3) };
        static int getUartIdx(USART_TypeDef* def) {
            if (def == USART3) return 2;
            else return 1;
        }
        static int getHandleIdx(UART_HandleTypeDef* handle) {
            if (handle->Instance == USART3) return 2;
            else return 1;
        }

        extern "C" {
            void HAL_UART_MspInit(UART_HandleTypeDef *uart) {
                s_drivers[getHandleIdx(uart)]._mspInit();
            }

            void HAL_UART_MspDeInit(UART_HandleTypeDef *uart) {
                s_drivers[getHandleIdx(uart)]._mspInit();
            }

            void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {}
            void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart) {}
            void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {}

            // Interrupts
            void USART3_IRQHandler() {
                s_drivers[getUartIdx(USART3)]._irq();
            }
        }

        Uart::Uart() : _idx(-1) {}
        Uart::Uart(const Pin& rx, const Pin& tx, int baud) : _idx(-1) {
            if (!rx.isValid() && !tx.isValid()) return;
            if ((rx == pins::NC || rx == pins::PA10 || rx == pins::PB7 || rx == pins::PB15) &&
                (tx == pins::NC || tx == pins::PA9 || tx == pins::PB6 || tx == pins::PB14)) {
                _idx = getUartIdx(USART1);
            }
            if ((rx == pins::NC || rx == pins::PD6 || rx == pins::PA3) &&
                (tx == pins::NC || tx == pins::PD5 || tx == pins::PA2)) {
                _idx = getUartIdx(USART2);
            }
            if ((rx == pins::NC || rx == pins::PD9 || rx == pins::PB11 || rx == pins::PC11) &&
                (tx == pins::NC || tx == pins::PD8 || tx == pins::PB10 || tx == pins::PC10)) {
                _idx = getUartIdx(USART3);
            }

            if (_idx >= 0) s_drivers[_idx].open(rx, tx, baud);
        }

        Uart::~Uart() {
        }

        bool
        Uart::isOpen() const {
            if (_idx >= 0) return s_drivers[_idx].isOpen();
            return false;
        }

        void
        Uart::close() {
            if (_idx >= 0) return s_drivers[_idx].close();
        }

        bool
        Uart::hasData() const {
            if (_idx >= 0) return s_drivers[_idx].hasData();
            return false;
        }

        size_t
        Uart::getReadWindow() const {
            if (_idx >= 0) {
                return s_drivers[_idx].getReadWindow();
            }
            return 0;
        }

        size_t
        Uart::getWriteWindow() const {
            if (_idx >= 0) {
                return s_drivers[_idx].getWriteWindow();
            }
            return 0;
        }

        void
        Uart::flush() {
            if (_idx >= 0) {
                s_drivers[_idx].flush();
            }
        }

uint16_t fletcher16(uint8_t *data, size_t count) {
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;
   size_t index;

   for( index = 0; index < count; ++index ) {
      sum1 = (sum1 + data[index]) % 255; // Original
      //sum1 = (sum1 + index | data[index]) % 255; // The "fix"
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}

        Conn&
        Uart::operator<<(const Msg& w) {
            Msg::Packet p = w.pack();
            if (_idx >= 0) {
                #ifdef USE_CHECKSUM
                uint8_t buf[11];
                #else
                uint8_t buf[9];
                #endif
                uint8_t header = w.getType() == Msg::STATUS ||
                                 w.getType() == Msg::ACK ? 0x02 : 0x03;
                buf[0] = header;
                buf[1] = p.buffer[0]; buf[2] = p.buffer[1];
                buf[3] = p.buffer[2]; buf[4] = p.buffer[3];
                buf[5] = p.buffer[4]; buf[6] = p.buffer[5];
                buf[7] = p.buffer[6]; buf[8] = p.buffer[7];
                #ifdef USE_CHECKSUM
                uint8_t checksum = fletcher16(&buf[1], 8);
                buf[9] = checksum & 0xFF;
                buf[10] = (checksum >> 8) & 0xFF;
                #endif
                s_drivers[_idx].write((uint8_t*) buf, sizeof(buf));
                //s_drivers[_idx].flush();
            }
            return *this;
        }

        Conn&
        Uart::operator>>(Msg& r) {
            Msg::Packet p;
            if (_idx >= 0) {
                r.setError(false);

                uint8_t header; // header
                uint16_t checksum; // fletcher16
                if (s_drivers[_idx].hadPartialRead()) {
                    while (header != 0x02) { // read until next (hopefully) transmission control
                        if (s_drivers[_idx].read((uint8_t*) &header, 1)) {
                            s_drivers[_idx].setPartialRead(true);
                            r.setError(true);
                            return *this;
                        }
                    }
                    
                    if (s_drivers[_idx].read((uint8_t*) p.buffer, sizeof(p.buffer))) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }

                    #ifdef USE_CHECKSUM
                    if (s_drivers[_idx].read((uint8_t*) &checksum, 2)) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }

                    uint16_t expected = fletcher16((uint8_t*) &p.buffer, sizeof(p.buffer));
                    if (checksum != expected) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }
                    #endif

                    s_drivers[_idx].setPartialRead(false);
                } else {
                    if (s_drivers[_idx].read((uint8_t*) &header, 1)) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }
                    if (header != 0x02 && header != 0x03) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }
                    if (s_drivers[_idx].read((uint8_t*) p.buffer, sizeof(p.buffer))) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }

                    #ifdef USE_CHECKSUM
                    if (s_drivers[_idx].read((uint8_t*) &checksum, 2)) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }
                    uint16_t expected = fletcher16((uint8_t*) &p.buffer, sizeof(p.buffer));
                    if (checksum != expected) {
                        s_drivers[_idx].setPartialRead(true);
                        r.setError(true);
                        return *this;
                    }
                    #endif
                }

            }
            r.unpack(p);
            return *this;
        }
    }
}


