#!/usr/bin/env python3

import argparse
import shlex
import os
import sys
import platform
from subprocess import Popen, PIPE

parser = argparse.ArgumentParser(description="Run GDB server.")
parser.add_argument("--exe", "-e", required=True, help="Path to the JLink GDB Server command line executable", dest="exe")
parser.add_argument("--device", "-d", required=True, help="The device to upload to, eg. STM32F777VI", dest="device")
args = parser.parse_args()

if args.exe == "JLINK_EXE-NOTFOUND":
    print("Could not find JLink exe")
    sys.exit(0)

# set system/version dependent "start_new_session" analogs
kwargs = {}
if platform.system() == 'Windows':
    # from msdn [1]
    CREATE_NEW_PROCESS_GROUP = 0x00000200  # note: could get it from subprocess
    DETACHED_PROCESS = 0x00000008  # 0x8 | 0x200 == 0x208
    kwargs.update(creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP)
elif sys.version_info < (3, 2):  # assume posix
    kwargs.update(preexec_fn=os.setsid)
else:  # Python 3.2+ and Unix
    kwargs.update(start_new_session=True)

command = "\"{}\" -select USB -device {} -if SWD -speed 1000 -ir -singlerun -strict".format(args.exe, args.device)
arguments = shlex.split(command)

print("Running GDB server!")
print(command)
p = Popen(arguments, stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr, **kwargs)
