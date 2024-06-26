#!/usr/bin/env python
import sys, re, subprocess

if len(sys.argv) == 1:
	in_fn = "arlib-debug.log"
elif len(sys.argv) == 2:
	in_fn = sys.argv[1]
else:
	1/0

lines = []  # [ ( "no address here", None, None ), ( "./myprogram(+0xab580)[0x5611372b4580]", "./myprogram", "0xab580" ) ]
addresses = {}  # { "./myprogram": { "0xab580": "arlib/whatever-file.cpp:550" } } (or value=None if not set yet)
for line in open(in_fn, "rt"):
	assert line.endswith("\n")
	line = line[:-1]
	if m := re.match("^(.*)\(\+(0x[0-9a-f]+)\)\[0x[0-9a-f]+\]$", line):
		lines.append( ( line, m[1], m[2] ) )
		addresses.setdefault(m[1], {})[m[2]] = None
	elif m := re.match("^(.*)\+(0x[0-9a-fA-F]+)$", line):
		lines.append( ( line, m[1], m[2] ) )
		addresses.setdefault(m[1], {})[m[2]] = None
	else:
		lines.append( ( line, None, None ) )

for fn,addr_map in addresses.items():
	addrs = list(addr_map.keys())
	if '.exe' in fn:
		for line in subprocess.run(["objdump", "-p", fn], capture_output=True).stdout.decode("utf-8").strip().split("\n"):
			if line.startswith("ImageBase"):
				base_addr = int(line[len("ImageBase"):].strip(),16)
		addrs2 = [hex(base_addr+int(n,16)) for n in addrs]
	else:
		addrs2 = addrs
	symbols = subprocess.run(["addr2line", "-e", fn] + addrs2, capture_output=True).stdout.decode("utf-8").strip().split("\n")
	assert len(symbols) == len(addrs)
	for addr, sym in zip(addrs, symbols):
		if sym != "??:?" and sym != "":
			addr_map[addr] = sym

for line, fn, addr in lines:
	if fn is not None and addresses[fn][addr] is not None:
		print(fn, addresses[fn][addr])
	else:
		print(line)
