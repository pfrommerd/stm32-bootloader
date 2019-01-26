#!/usr/bin/env python3
import argparse
import os

parser = argparse.ArgumentParser(description="Generate gdb makefile.")
parser.add_argument("symbol", help="The .elf file to use for debugging.")
args = parser.parse_args()

print("Generating .gdbinit")
contents = """define target hookpost-remote
file \"{}\"
monitor reset
end
""".format(args.symbol)

home = None
if "HOME" in os.environ:
    home = os.environ["HOME"]
else:
    print("Could not find home directory")
    sys.exit(0)

with open(home + "/.gdbinit", 'w') as f:
    f.write(contents)
