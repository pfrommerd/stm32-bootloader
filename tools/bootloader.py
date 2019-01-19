#!/usr/bin/env python3

import itertools
from msg import *
import math
import time

# Wrapper for a board type
class Board:
    def __init__(self, dev, bid):
        self._conn = Conn(dev, bid)
        self._id = bid

    @property
    def conn(self):
        return self._conn

    def status(self):
        ack = self._conn.status()
        return ack['payload'][3]

    # Transmission controlled commands

    def reset(self):
        self._conn.query(CmdType.RESET)

    def set_mode(self, mode):
        self._conn.query(cmd=CmdType.SET_MODE, payload=[mode.value, 0, 0, 0])

    def get_mode(self):
        resp = self._conn.query(CmdType.GET_MODE)
        return Mode(resp['value'])

    def unlock_flash(self):
        self._conn.query(CmdType.UNLOCK_FLASH)

    def lock_flash(self):
        self._conn.query(CmdType.UNLOCK_FLASH)

    def move(self, pos):
        self._conn.query(CmdType.MOVE, value=pos)

    def move_start(self):
        msg = self._conn.query(CmdType.MOVE_START)
        return msg['value'] # The start address

    def position(self):
        msg = self._conn.query(CmdType.POSITION)
        return msg['value'] # Current position

    def read(self, length=4):
        data = bytes()
        blocks = int((length + 3)/4)
        for i in range(blocks):
            msg = self._conn.query(CmdType.READ)
            data = data + msg['payload']
        return data

    def write(self, data):
        if len(data) > 4:
            data = data[:4]
        if len(data) < 4:
            data = data + bytes([0] * (4 - len(data)))
        self._conn.write(CmdType.WRITE, payload=data)

    def erase(self, length):
        self._conn.query(CmdType.ERASE, value=length, timeout=20);

    # Does the whole flashing rigmarole
    def load(self, data, write_callback = lambda i,b: None):
        self.unlock_flash()
        # Move to the start of the flash block
        start_pos = self.move_start()
        blocks = int((len(data) + 3)/4)
        repeated = [iter(data)] * 4
        packets = map(lambda x: bytes(x), itertools.zip_longest(*repeated, fillvalue=0))
        for i, packet in enumerate(packets):
            position = start_pos + 4 * i
            # Every 256 bytes check that our write position
            # is still correct
            if DEBUG and i % 256 == 0:
                actual_pos = self.position()
                if actual_pos != position:
                    raise IOError('Mismatched positions 0x{:08x} and 0x{:08x}' \
                                    .format(position, actual_pos))

            if DEBUG: print('writing 0x{:08x}: {}'.format(position, packet.hex()))
            self.write(packet)
            write_callback(i + 1, blocks)
                
        self.lock_flash()

if __name__=='__main__':
    # Run the application!
    import serial
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--dev", help="The USB device to connect to", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, help="The baud rate", default=921600)
    parser.add_argument("--print_stream", help="Just read out the incoming stream", action="store_true")

    parser.add_argument("--id", type=int, help="The ID of the target board", default=1)

    parser.add_argument("--reset", help="Reset the controller", action="store_true")

    parser.add_argument("--get_mode", help="Get boot mode", action="store_true")
    parser.add_argument("--set_mode_app", help="Set flag to boot into application", action="store_true")
    parser.add_argument("--set_mode_bootloader", help="Set flag to boot into application", action="store_true")

    parser.add_argument("--move", type=int, help="Move to a certain position", default=-1)
    parser.add_argument("--move_start", help="Move to a certain position", action="store_true")
    parser.add_argument("--read", type=int, help="Read a certain number of bytes",
                                    nargs='?', const=4, default=-1)
    parser.add_argument("--write", type=str, help="Write 4 bytes", default="")

    parser.add_argument("--erase", type=int, help="Wipes the app flash memory", nargs='?', const=0, default=-1)
    parser.add_argument("--dump", type=int, help="Dumps a certain number of bytes from the start \
                                                  to a file", nargs='?', const=4, default=-1)
    parser.add_argument("--load", type=str, help="Writes a file into the app flash memory", default="")
    args = parser.parse_args()

    load_data = None
    if len(args.load) > 0:
        with open(args.load, 'rb') as fh:
            load_data = fh.read()

    device = Port(args.dev, args.baud)

    if args.print_stream:
        while True:
            print(read(device))


    board = Board(device, args.id)

    # Mode related things
    if args.set_mode_app:
        board.set_mode(Mode.APP)
    if args.set_mode_bootloader:
        board.set_mode(Mode.BOOTLOADER)
    if args.get_mode:
        print(board.get_mode())

    # Read-write related things
    if args.move_start:
        board.move_start()
    if args.move >= 0:
        board.move(args.move)
    if args.read > 0:
        print(board.read(args.read).hex())

    if args.dump > 0:
        board.move_start()
        print(board.read(args.dump)[:args.dump].hex())

    if args.erase >= 0:
        num_bytes = args.erase
        if num_bytes == 0 and load_data is not None:
            num_bytes = len(load_data)
        print('Erasing...')
        if num_bytes > 0:
            board.erase(num_bytes)
        print('Erased')
    
    if len(args.write) > 0:
        board.unlock_flash()
        board.write(bytes.fromhex(args.write))
        board.lock_flash()

    if load_data is not None:
        start = time.time()
        if DEBUG:
            board.load(load_data)
        else:
            board.load(load_data, lambda i, b: \
                    print('Wrote block {}/{} (tr: {:5.0f} mps, ti: {:5.8f}s, bt: {:3d})' \
                            .format(i, b, board.conn.transmission_rate, \
                                    board.conn.transmission_interval,
                                    board.conn.bad_transmits), end='\r'))
        print()
        elapsed = time.time() - start
        print('Flashed at {} bps'.format(len(load_data) / elapsed))

    if args.reset:
        board.reset()
