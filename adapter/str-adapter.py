import sys

def read64le(data, offset):
	return ord(data[offset]) | (ord(data[offset + 1]) << 8) | (ord(data[offset + 2]) << 16) | (ord(data[offset + 3]) << 24) | (ord(data[offset + 4]) << 32) | (ord(data[offset + 5]) << 40) | (ord(data[offset + 6]) << 48) | (ord(data[offset + 7]) << 56)

reccord = {}
while True:
	data = sys.stdin.read(8)
	if data:
		size = read64le(data, 0)
		motif = sys.stdin.read(size)
		print repr(motif)
	else:
		break
