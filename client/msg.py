import time
import struct
from enum import Enum
import math
import serial

DEBUG=False
USE_CHECKSUM=True
PACKET_LEN = 11 if USE_CHECKSUM else 9

class AutoNumberEnum(Enum):
     def __new__(cls):
        value = len(cls.__members__)  # note no + 1
        obj = object.__new__(cls)
        obj._value_ = value
        return obj

# Note: Keep in line with c++ code!
class CmdType(AutoNumberEnum):
    INVALID = ()
    STATUS = ()
    ACK = ()
    OKAY = ()
    ERROR = ()
    PING = ()
    RESET = ()
    GET_MODE = ()
    SET_MODE = ()
    CONN_STATUS_REQ = ()
    CONN_STATUS = ()
    ERASE = ()
    CHECKSUM = ()
    UNLOCK_FLASH = ()
    LOCK_FLASH = ()
    MOVE = ()
    MOVE_START = ()
    POSITION = ()
    READ = ()
    WRITE = ()

class Mode(Enum):
    APP = 0
    BOOTLOADER = 1

def fletcher16(data):
    sum1 = 0
    sum2 = 0
    for x in data:
        sum1 = (sum1 + int(x)) % 255
        sum2 = (sum2 + sum1) % 255
    return (sum2 << 8) | sum1

def pack_msg(cmd):
    board_id = cmd['board_id']
    c = cmd['cmd']
    if isinstance(c, str):
        c = ord(c[0])
    if isinstance(c, CmdType):
        c = c.value
    length = 4
    seq_num = 0
    if 'seq_num' in cmd:
        seq_num = cmd['seq_num']
    payload = bytes(cmd['payload'] if 'payload' in cmd and cmd['payload'] else [0, 0, 0, 0])
    if 'value' in cmd and cmd['value'] is not None:
        payload = struct.pack('<L', cmd['value'])

    header = 0x02 if c == CmdType.STATUS.value or \
                     c == CmdType.ACK.value else 0x03

    packet = struct.pack('<BBBB', board_id, c, length, seq_num) + payload
    tail = struct.pack('<H', fletcher16(packet)) if USE_CHECKSUM else bytes()
    # Slap a very simple crc on the whole thing
    packet = bytes([header])  + packet + tail
    return packet

def unpack_msg(packet):
    header, board_id, c, length, seq_num = struct.unpack('<BBBBB', packet[:5])
    payload = packet[5:9]
    value = struct.unpack('<L', payload)[0]
    return {'board_id': board_id, 'cmd': CmdType(c), 'length': length,
            'seq_num': seq_num, 'payload': payload, 'value': value}
class Port:
    def __init__(self, port, baud):
        self._dev = serial.Serial(port, baud, timeout=None)

    def reset_read_buffer(self):
        self._dev.reset_input_buffer()

    def try_read(self):
        if self._dev.in_waiting < 9:
            return None
        packet = self._dev.read(9)
        if DEBUG: print('r {}'.format(packet.hex()))
        return unpack_msg(packet)

    def read(self, timeout=-1):
        if timeout > 0:
            t = time.time()
            while time.time() - t < timeout and self._dev.in_waiting < PACKET_LEN:
                pass
            if self._dev.in_waiting < PACKET_LEN:
                return None
        packet = self._dev.read(PACKET_LEN)
        if DEBUG: print('r {}'.format(packet.hex()))
        return unpack_msg(packet)

    def write(self, msg):
        packet = pack_msg(msg)
        if DEBUG: print('w {}'.format(packet.hex()))
        self._dev.write(packet)
        self._dev.flush()

class Status(Enum):
    OUTSTANDING = 0
    COMPLETE = 1
    FAILURE = 2
    SUCCESS = 3

class Conn:
    def __init__(self, port, board_id):
        self._seq_num = 0
        self._id = board_id
        self._port = port

        self._bad_transmits = 0
        self._quiet_time = 0.0002
        self._seq_num = self.status()

        # Should not be longer than 255
        # (or seq numbers might collide)
        self._flush_interval = 64

        self._outstanding = []

    def _bad_transmission(self):
        self._bad_transmits = self._bad_transmits + 1
        #self._quiet_time = max(0.000001, self._quiet_time * 1.2)

    def _good_transmission(self):
        pass
        #self._quiet_time = max(0.000001, self._quiet_time * 0.99)

    @property
    def transmission_interval(self):
        return self._quiet_time

    @property
    def transmission_rate(self):
        return 1 / (self._quiet_time)

    @property
    def bad_transmits(self):
        return self._bad_transmits

    def status(self):
        msg = {'board_id': self._id, 'cmd': CmdType.STATUS}
        time.sleep(2*self._quiet_time)
        self._port.write(msg)
        ack = self._port.read(timeout=0.002)

        if ack is None or ack['cmd'] != CmdType.ACK:
            quiet_time = self._quiet_time
            while ack is None:
                if DEBUG: print('failed to get status')
                time.sleep(quiet_time)
                self._port.reset_read_buffer()
                quiet_time = 2 * quiet_time
                self._port.write(msg)
                ack = self._port.read(timeout=0.002)
        return ack['payload'][3]

    def write(self, cmd, payload=None, value=None):
        action = { 'status': Status.OUTSTANDING }

        def write_action(run=False):
            msg = {'board_id': self._id, 'seq_num': self._seq_num,
                    'cmd': cmd, 'payload': payload, 'value': value}

            time.sleep(self._quiet_time)
            self._port.write(msg)

            action['seq_num'] = self._seq_num
            action['status'] = Status.COMPLETE

            self._seq_num = (self._seq_num + 1) % 256


        action['run'] = write_action
        self.do(action)

    def query(self, cmd, payload=None, value=None, timeout=1):
        action = { 'status': Status.OUTSTANDING }

        result = None

        def query_action():
            msg = {'board_id': self._id, 'seq_num': self._seq_num,
                    'cmd': cmd, 'payload': payload, 'value': value}

            time.sleep(self._quiet_time)
            self._port.write(msg)

            nonlocal result
            result = self._port.read(timeout=timeout)

            action['seq_num'] = self._seq_num
            action['status'] = Status.SUCCESS if result is not None else Status.FAILURE

            self._seq_num = (self._seq_num + 1) % 256

        action['run'] = query_action
        self.do(action)

        return result

    def do(self, action):
        if action['status'] != Status.SUCCESS:
            self._outstanding.append(action)
            action['run']()
            if action['status'] == Status.FAILURE:
                self.flush()
        if action['seq_num'] % self._flush_interval == 0:
            self.flush()

    def _clear_successful(self):
        now = time.time()
        next_seq = self.status()
        dt = time.time() - now
        if DEBUG: print('status request took {}'.format(dt))
        last_seq = (next_seq - 1) % 256

        # Go through the outstanding
        # actions and mark the one
        # with the return sequence number
        # as successful
        last_success = -1
        for i, action in enumerate(self._outstanding):
            if action['status'] == Status.COMPLETE and \
                    action['seq_num'] == last_seq:
                last_success = i
            if action['status'] == Status.SUCCESS:
                last_success = i
            if action['status'] == Status.FAILURE:
                break

        self._outstanding = self._outstanding[last_success + 1:]
        return next_seq

    # Request an ack
    # and retransmit anything that failed
    def flush(self):
        next_seq = self._clear_successful()

        if len(self._outstanding) == 0:
            self._good_transmission()
            if DEBUG: print('good transmission {:02x} {:02x} (quiet time {:9.6f})'.format(next_seq, self._seq_num, self._quiet_time))
        else:
            self._bad_transmission()
            if DEBUG: print('bad transmission {:02x} {:02x}, missed {} (quiet time {:9.6f})'.format(next_seq, self._seq_num, len(self._outstanding), self._quiet_time))

        while len(self._outstanding) > 0:
            # Reset the sequence number
            self._seq_num = next_seq

            # Re-run anything oustanding
            for action in self._outstanding:
                if DEBUG: print('retransmitting {:x}'.format(action['seq_num']))
                action['run']()
            next_seq = self._clear_successful()
