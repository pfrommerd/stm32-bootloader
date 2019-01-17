#include "Can.hpp"
#include "Buffer.hpp"
#include <stm32f7xx_hal.h>

namespace bootloader {
    namespace can {
        struct CanMsg {
            bool ext; // Identifier extension bit for extended can
            bool remote; // Remote transmission req. bit
            uint32_t id; // Message ID (only 11 bits unless IDE set)
            uint8_t length; // Number of bytes
            uint8_t data[8]; // Message data
        };

        // The actual backend for each
        // can bus
        class CanDriver {
        public:
            CanDriver(CAN_TypeDef* can) {
                _handle.Instance = can;
                _error = false;
            }

            void open(const Pin& rx, const Pin& tx, int baud) {
                _rxPin = rx;
                _txPin = tx;

                _handle.State = HAL_CAN_STATE_RESET;
                _handle.Init.Prescaler = 6000000 / baud;
                _handle.Init.Mode = CAN_MODE_NORMAL;
                _handle.Init.TimeSeg1 = CAN_BS1_6TQ;//consider increasing or decreasing, but not sure
                _handle.Init.TimeSeg2 = CAN_BS2_2TQ;
                _handle.Init.TimeTriggeredMode = DISABLE;
                _handle.Init.AutoBusOff = ENABLE;
                _handle.Init.AutoWakeUp = DISABLE;
                _handle.Init.AutoRetransmission = ENABLE;
                _handle.Init.ReceiveFifoLocked = DISABLE;
                _handle.Init.TransmitFifoPriority = DISABLE;

                if (HAL_CAN_Init(&_handle) != HAL_OK) {
                    asm("bkpt 255");
                    return;
                }

                CAN_FilterTypeDef filters;
                filters.FilterIdHigh = 0;
                filters.FilterIdLow = 0;
                filters.FilterMaskIdHigh = 0;
                filters.FilterMaskIdLow = 0;
                filters.FilterFIFOAssignment = CAN_FILTER_FIFO0;
                filters.FilterBank = 0;
                filters.FilterMode = CAN_FILTERMODE_IDMASK;
                filters.FilterScale = CAN_FILTERSCALE_16BIT;
                filters.FilterActivation = ENABLE;
                filters.SlaveStartFilterBank = 0;

                if (HAL_CAN_ConfigFilter(&_handle, &filters) != HAL_OK) {
                    asm("bkpt 255");
                    return;
                }

                filters.FilterScale = CAN_FILTERSCALE_32BIT;
                filters.FilterBank = 1;
                filters.SlaveStartFilterBank = 1;

                if (HAL_CAN_ConfigFilter(&_handle, &filters) != HAL_OK) {
                    asm("bkpt 255");
                    return;
                }

                if (HAL_CAN_Start(&_handle) != HAL_OK) {
                    asm("bkpt 255");
                    return;
                }

                if (HAL_CAN_ActivateNotification(&_handle,
                                         CAN_IT_TX_MAILBOX_EMPTY |
                                         CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING)
                                            != HAL_OK) {
                    asm("bkpt 255");
                    return;
                }
            }

            void close() {
                if (HAL_CAN_Stop(&_handle) != HAL_OK) asm("bkpt 255");
                if (HAL_CAN_DeInit(&_handle) != HAL_OK) asm("bkpt 255");
            }

            // Msp init and deinit functions
            void _mspInit() {
                uint8_t gpioAf;
                if (_handle.Instance == CAN1) {
                    __HAL_RCC_CAN1_CLK_ENABLE();
                    gpioAf = GPIO_AF9_CAN1;
                } else if (_handle.Instance == CAN2) {
                    __HAL_RCC_CAN1_CLK_ENABLE();
                    __HAL_RCC_CAN2_CLK_ENABLE();
                    gpioAf = GPIO_AF9_CAN2;
                }
                #ifdef CAN3
                else if (_handle.Instance == CAN3) {
                    __HAL_RCC_CAN3_CLK_ENABLE();
                    gpioAf = GPIO_AF11_CAN3;
                }
                #endif

                _txPin.init(Pin::Mode::ALT_PUSH_PULL, Pin::Pull::NONE, Pin::Speed::VERY_HIGH,
                                    gpioAf);
                _rxPin.init(Pin::Mode::ALT_PUSH_PULL, Pin::Pull::UP, Pin::Speed::VERY_HIGH,
                                   gpioAf);

                // Setup interrupts
                _irqEnable();
            }

            void _irqEnable() {
                if (_handle.Instance == CAN1) {
                    HAL_NVIC_SetPriority(CAN1_TX_IRQn,6,5);
                    HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
                    HAL_NVIC_SetPriority(CAN1_RX0_IRQn,6,5);
                    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
                    HAL_NVIC_SetPriority(CAN1_RX1_IRQn,6,5);
                    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
                }
            }

            void _irqDisable() {
                HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
                HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
                HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
            }

            void _mspDeInit() {
                _irqDisable();
                if (_handle.Instance == CAN1) {
                    __HAL_RCC_CAN1_CLK_DISABLE();
                } else if (_handle.Instance == CAN2) {
                    __HAL_RCC_CAN1_CLK_DISABLE();
                    __HAL_RCC_CAN2_CLK_DISABLE();
                }
            }

            // IRQs

            // On successful transmission
            void _txIRQ() {
                uint32_t tsr = _handle.Instance->TSR;
                if (tsr & CAN_TSR_RQCP0)
                    __HAL_CAN_CLEAR_FLAG(&_handle, CAN_FLAG_RQCP0);
                if (tsr & CAN_TSR_RQCP1)
                    __HAL_CAN_CLEAR_FLAG(&_handle, CAN_FLAG_RQCP1);
                if (tsr & CAN_TSR_RQCP2)
                    __HAL_CAN_CLEAR_FLAG(&_handle, CAN_FLAG_RQCP2);
                // Transmit the next element in the queue, if any
                if (!_txBuf.empty()) {
                    _transmit(_txBuf.pop());
                } else {
                    _transmitting = false;
                }
            }

            void _rxIRQ(int fifo) {
                CanMsg msg;
                msg.ext = CAN_RI0R_IDE & _handle.Instance->sFIFOMailBox[fifo].RIR;
                if (msg.ext) {
                    msg.id = ((CAN_RI0R_EXID | CAN_RI0R_STID) &
                                _handle.Instance->sFIFOMailBox[fifo].RIR) >> CAN_RI0R_EXID_Pos;
                } else {
                    msg.id = (CAN_RI0R_STID &
                                _handle.Instance->sFIFOMailBox[fifo].RIR) >> CAN_RI0R_EXID_Pos;
                }
                msg.remote = CAN_RI0R_RTR & _handle.Instance->sFIFOMailBox[fifo].RIR;
                msg.length = (CAN_RDT0R_DLC &
                                _handle.Instance->sFIFOMailBox[fifo].RDTR) >> CAN_RDT0R_DLC_Pos;

                msg.data[0] = (CAN_RDL0R_DATA0 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDLR) >> CAN_RDL0R_DATA0_Pos;
                msg.data[1] = (CAN_RDL0R_DATA1 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDLR) >> CAN_RDL0R_DATA1_Pos;
                msg.data[2] = (CAN_RDL0R_DATA2 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDLR) >> CAN_RDL0R_DATA2_Pos;
                msg.data[3] = (CAN_RDL0R_DATA3 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDLR) >> CAN_RDL0R_DATA3_Pos;
                msg.data[4] = (CAN_RDH0R_DATA4 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDHR) >> CAN_RDH0R_DATA4_Pos;
                msg.data[5] = (CAN_RDH0R_DATA5 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDHR) >> CAN_RDH0R_DATA5_Pos;
                msg.data[6] = (CAN_RDH0R_DATA6 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDHR) >> CAN_RDH0R_DATA6_Pos;
                msg.data[7] = (CAN_RDH0R_DATA7 & 
                                _handle.Instance->sFIFOMailBox[fifo].RDHR) >> CAN_RDH0R_DATA7_Pos;
                if (fifo == CAN_RX_FIFO0) {
                    SET_BIT(_handle.Instance->RF0R, CAN_RF0R_RFOM0);
                } else if (fifo == CAN_RX_FIFO1) {
                    SET_BIT(_handle.Instance->RF1R, CAN_RF1R_RFOM1);
                }

                // Save to rx buffer
                // we're in an interrupt
                // so no one can interfere
                _rxBuf.push(msg);
            }

            void _transmit(const CanMsg& msg) {
                _transmitting = true;
                const uint32_t mailbox = (_handle.Instance->TSR & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos;
                const uint8_t remoteTr = msg.remote ? CAN_RTR_REMOTE : CAN_RTR_DATA;
                if (msg.ext) {
                    _handle.Instance->sTxMailBox[mailbox].TIR = ((msg.id << CAN_TI0R_EXID_Pos) |
                                                                  CAN_ID_EXT | remoteTr);
                } else {
                    _handle.Instance->sTxMailBox[mailbox].TIR = ((msg.id << CAN_TI0R_STID_Pos) |
                                                                  remoteTr);
                }
                _handle.Instance->sTxMailBox[mailbox].TDTR = msg.length;

                WRITE_REG(_handle.Instance->sTxMailBox[mailbox].TDHR,
                          (static_cast<uint32_t>(msg.data[7]) << CAN_TDH0R_DATA7_Pos) |
                          (static_cast<uint32_t>(msg.data[6]) << CAN_TDH0R_DATA6_Pos) |
                          (static_cast<uint32_t>(msg.data[5]) << CAN_TDH0R_DATA5_Pos) |
                          (static_cast<uint32_t>(msg.data[4]) << CAN_TDH0R_DATA4_Pos));
                WRITE_REG(_handle.Instance->sTxMailBox[mailbox].TDLR,
                          (static_cast<uint32_t>(msg.data[3]) << CAN_TDL0R_DATA3_Pos) |
                          (static_cast<uint32_t>(msg.data[2]) << CAN_TDL0R_DATA2_Pos) |
                          (static_cast<uint32_t>(msg.data[1]) << CAN_TDL0R_DATA1_Pos) |
                          (static_cast<uint32_t>(msg.data[0]) << CAN_TDL0R_DATA0_Pos));
                SET_BIT(_handle.Instance->sTxMailBox[mailbox].TIR, CAN_TI0R_TXRQ);
            }

            bool write(const CanMsg &msg) {
                // Wait until space to transmit
                while (_txBuf.full()) {
                    if (!_transmitting) {
                        _transmit(_txBuf.pop());
                        break;
                    }
                }
                bool pushed = _txBuf.push(msg);
                if (!_transmitting) {
                    _transmit(_txBuf.pop());
                }
            }

            void clearBuffer() {
                _rxBuf.clear();
            }

            void flush() {
                // Sit around until everything is done
                // transmitting
                while (_transmitting) {}
            }

            bool hasData() const { // If there is a message in the line
                return !_rxBuf.empty();
            }


            void read(CanMsg *dst) {
                while (!hasData()) {}
                // No interrupting while we read from the buffer
                //_irqDisable();
                *dst = _rxBuf.pop();
                //_irqEnable();
            }

            size_t getReadWindow() const {
                return _rxBuf.free();
            }
            size_t getWriteWindow() const {
                return _txBuf.free();
            }
        private:
            CAN_HandleTypeDef _handle;
            Pin _rxPin;
            Pin _txPin;
            Buffer<CanMsg, 256> _rxBuf;
            Buffer<CanMsg, 256> _txBuf;
            bool _transmitting;
            bool _error;
        };

        // The various drivers (global)
#ifdef CAN3
        static CanDriver s_drivers[3] = { CanDriver(CAN1), CanDriver(CAN2), CanDriver(CAN3) };
#else
        static CanDriver s_drivers[2] = { CanDriver(CAN1), CanDriver(CAN2) };
#endif
        static int getCanIdx(CAN_TypeDef* def) {
            if (def == CAN1) return 0;
            else if (def == CAN2) return 1;
#ifdef CAN3
            else if (def == CAN3) return 2;
#endif
            else return 0;
        }
        static int getHandleIdx(CAN_HandleTypeDef* handle) {
            return getCanIdx(handle->Instance);
        }

        // Interrupts
        extern "C" {
            void HAL_CAN_MspInit(CAN_HandleTypeDef* handle) {
                s_drivers[getHandleIdx(handle)]._mspInit();
            }
            void HAL_CAN_MspDeInit(CAN_HandleTypeDef* handle) {
                s_drivers[getHandleIdx(handle)]._mspInit();
            }
            void CAN1_TX_IRQHandler() {
                s_drivers[getCanIdx(CAN1)]._txIRQ();
            }
            void CAN1_RX0_IRQHandler() {
                s_drivers[getCanIdx(CAN1)]._rxIRQ(CAN_RX_FIFO0);
            }
            void CAN1_RX1_IRQHandler() {
                s_drivers[getCanIdx(CAN1)]._rxIRQ(CAN_RX_FIFO1);
            }
            void CAN1_SCE_IRQHandler() {} // Not used
        }

        // EXTERNAL API:
        Can::Can() : _idx(-1) {}

        Can::Can(const Pin& rx, const Pin& tx, int baud) : _idx(-1) {
            // Get the correct can number
            if ((rx == pins::PI9 || rx == pins::PH14 || rx == pins::PA11
                        || rx == pins::PD0 || rx == pins::PB8) &&
                (tx == pins::PA12 || tx == pins::PH13
                        || tx == pins::PD1 || tx == pins::PB9)) {
                _idx = 0;
            } else if ((rx == pins::PB12 || rx == pins::PB5) &&
                        (tx == pins::PB13 || tx == pins::PB6)) {
                _idx = 1;
            }
            #ifdef CAN3
            else if ((rx == pins::PA8 || rx == pins::PB3) &&
                     (tx == pins::PA15 || tx == pins::PB4)) {
                _idx = 2;
            }
            #endif
            if (_idx >= 0) s_drivers[_idx].open(rx, tx, baud);
        }
        Can::~Can() {}

        void
        Can::close() {
            if (_idx >= 0) s_drivers[_idx].close();
            _idx = -1;
        }

        bool
        Can::isOpen() const {
            return _idx >= 0;
        }

        bool
        Can::hasData() const {
            if (_idx >= 0) {
                return s_drivers[_idx].hasData();
            }
            return false;
        }

        size_t
        Can::getReadWindow() const {
            if (_idx >= 0) {
                return s_drivers[_idx].getReadWindow();
            }
            return 0;
        }

        size_t
        Can::getWriteWindow() const {
            if (_idx >= 0) {
                return s_drivers[_idx].getWriteWindow();
            }
            return 0;
        }

        void
        Can::flush() {
            if (_idx >= 0) {
                s_drivers[_idx].flush();
            }
        }


        Conn&
        Can::operator<<(const Msg& w) {
            Msg::Packet p = w.pack();
            // Make a CAN frame
            CanMsg m;
            m.remote = false;
            m.ext = false;
            m.id = 1;
            m.length = 8;
            m.data[0] = p.buffer[0]; m.data[1] = p.buffer[1];
            m.data[2] = p.buffer[2]; m.data[3] = p.buffer[3];
            m.data[4] = p.buffer[4]; m.data[5] = p.buffer[5];
            m.data[6] = p.buffer[6]; m.data[7] = p.buffer[7];
            if (_idx >= 0) {
                s_drivers[_idx].write(m);
                //s_drivers[_idx].flush();
            }
            return *this;
        }

        Conn&
        Can::operator>>(Msg& r) {
            if (_idx >= 0) {
                CanMsg m;
                s_drivers[_idx].read(&m);
                Msg::Packet p;
                p.buffer[0] = m.data[0]; p.buffer[1] = m.data[1];
                p.buffer[2] = m.data[2]; p.buffer[3] = m.data[3];
                p.buffer[4] = m.data[4]; p.buffer[5] = m.data[5];
                p.buffer[6] = m.data[6]; p.buffer[7] = m.data[7];
                r.unpack(p);
            }
            return *this;
        }
    }
}
