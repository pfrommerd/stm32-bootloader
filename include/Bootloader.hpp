#pragma once

#include <cstddef>
#include <cinttypes>
#include <array>

#include "Buffer.hpp"

namespace bootloader {
    typedef uint8_t board_id;

    enum class Mode {
        APP = 0,
        BOOTLOADER = 1
    };

    Mode getMode();
    void setMode(Mode m);

    class Msg {
    public:
        // An 8-byte serialized
        // representation of this class
        union Packet {
            struct {
                uint8_t id;
                uint8_t type;
                uint8_t length; // Payload length
                uint8_t seqNum; // A sequence number to detect dropped packets
                uint8_t data[4]; // The data
            } fields;
            uint8_t buffer[8]; // The original buffer
        };

        enum Type {
            INVALID = 0,

            // No sequence number checking/incrementing
            // is performed on the following
            // as these are all used for retransmission control:

            // 0x02 header
            STATUS, // Just meant to be acked, nothing else
            ACK, // contains expected next seq num
            
            // These are all sequence controlled:

            // 0x03 header

            // Generic command response
            OKAY,
            ERROR,

            // used for discovering who is up
            PING,

            // reset/mode control
            RESET,
            GET_MODE,
            SET_MODE,

            // to get transmission rate
            // information for connections (for display/debug)
            CONN_STATUS_REQ, // get status of all connections
            CONN_STATUS, // data contains conn id, conn type, rx window, tx window

            // flash/write/read control
            ERASE, // Will send back OKAY when done
            CHECKSUM, // Will send back checksum (to see if we need to overwrite)
            UNLOCK_FLASH, // Will send back OKAY
            LOCK_FLASH, // Will send back OKAY
            MOVE, // Will send back new position in OKAY
            MOVE_START, // Will send back move position in OKAY
            POSITION, // Will send back postion in OKAY
            READ, // Will send back data in OKAY
            WRITE // Will not send anything back
        };

        inline constexpr Msg(board_id id, Type type, uint8_t seqNum, uint8_t len,
                                const std::array<uint8_t, 4> &data) : _boardId(id), _type(type),
                                                                      _seqNum(seqNum), _length(len),
                                                                      _data(data), _error(false) {}
        inline constexpr Msg() : _boardId(0), _type(INVALID), _seqNum(0), _length(0), _data(),
                                 _error(false) {}
        inline constexpr Msg(bool error) : _boardId(0), _type(INVALID), _seqNum(0), _length(0), _data(),
                                           _error(error) {}

        inline uint8_t getSeqNum() const { return _seqNum; }
        inline void setSeqNum(uint8_t seq) { _seqNum = seq; }

        inline void setError(bool error) { _error = error; }
        inline bool hasError() const { return _error; }

        inline void setID(board_id id) { _boardId = id; }
        inline board_id getID() const { return _boardId; }

        inline void setType(Type type) { _type = type; }
        inline Type getType() const { return _type; }

        inline void setLength(uint8_t length) { _length = length; }
        inline uint8_t getLength() const { return _length; }

        inline const std::array<uint8_t, 4>& getData() const { return _data; }
        inline const uint8_t& getData(size_t idx) const { return _data[idx]; }
        inline uint8_t& getData(size_t idx) { return _data[idx]; }

        inline void setData(size_t idx, uint8_t val) { _data[idx] = val; }
        inline void setData(const std::array<uint8_t, 4>&d) { _data = d; }

        // Sets a single little-endian value
        inline void setValue(uint32_t data) {
            _data[0] = data & 0xFF; _data[1] = data >> 8 & 0xFF;
            _data[2] = data >> 16 & 0xFF; _data[3] = data >> 24 & 0xFF;
        }
        // Gets a single little-endian value
        inline uint32_t getValue() const {
            return (uint32_t) _data[0] | ((uint32_t) _data[1] << 8) |
                   ((uint32_t) _data[2] << 16) | ((uint32_t) _data[3] << 24);
        }

        // For serialization/deserialization
        inline Packet pack() const {
            Packet p;
            p.fields.id = static_cast<uint8_t>(_boardId);
            p.fields.type = static_cast<uint8_t>(_type);
            p.fields.seqNum = _seqNum;
            p.fields.length = _length;
            p.fields.data[0] = _data[0];
            p.fields.data[1] = _data[1];
            p.fields.data[2] = _data[2];
            p.fields.data[3] = _data[3];
            return p;
        }
        inline void unpack(const Packet& p) {
            _boardId = static_cast<board_id>(p.fields.id);
            _type = static_cast<Type>(p.fields.type);
            _seqNum = p.fields.seqNum;
            _length = p.fields.length;
            _data = {p.fields.data[0], p.fields.data[1],
                     p.fields.data[2], p.fields.data[3]};
            _error = false;
        }

    private:
        board_id _boardId;
        Type _type;
        uint8_t _seqNum;
        uint8_t _length;
        std::array<uint8_t, 4> _data;
        bool _error; // For read error, not actually part of the message
    };

    // To be overridden by
    // different connection types
    // (i.e can, uart, etc.)
    class Conn {
    public:
        inline virtual ~Conn() {}

        virtual bool isOpen() const = 0;
        virtual void close() = 0;

        virtual bool hasData() const = 0;

        // Free space in the read queue
        virtual size_t getReadWindow() const = 0;
        // Free space in the write queue
        virtual size_t getWriteWindow() const = 0;

        virtual void flush() = 0;

        virtual Conn& operator>>(Msg &r) = 0; // Read
        virtual Conn& operator<<(const Msg &w) = 0; // Write
    };

    class Context {
    public:
        Context(uint8_t* appStart, int boardId,
                    Conn** conns, int numConns);

        void exec(const Msg& cmd, Conn* conn); // Execute a command, returns an error or ok messagek

        void run(); // Runs the bootloader in this context
    private:
        // Board config related things
        board_id _boardId;
        uint8_t* _appStart;
        Conn** _conns;
        int _numConns;

        // Transmission state
        uint8_t _seqNum; // Current sequence number
        Buffer<Msg, 32> _history; // for debugging

        // Command-related stuff
        bool _resetReq;
        bool _isWriting;
        uint8_t* _position;
    };
}
