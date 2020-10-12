import sys

def read64le(data, offset):
	return ord(data[offset]) | (ord(data[offset + 1]) << 8) | (ord(data[offset + 2]) << 16) | (ord(data[offset + 3]) << 24) | (ord(data[offset + 4]) << 32) | (ord(data[offset + 5]) << 40) | (ord(data[offset + 6]) << 48) | (ord(data[offset + 7]) << 56)

def getname(reccord, motif):
	key = str(len(motif))
	if key in reccord:
		reccord[key] += 1
	else:
		reccord[key] = 1
	return '$str%03u_%03u' % (len(motif), reccord[key])

def yarastr(motif):
	HEX = ('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F')
	return '{' + ' '.join([HEX[ord(b) >> 4] + HEX[ord(b) & 0xf] for b in motif]) + '}'

reccord = {}
while True:
	data = sys.stdin.read(8)
	if data:
		size = read64le(data, 0)
		motif = sys.stdin.read(size)
		print getname(reccord, motif) + ' = ' + yarastr(motif)
	else:
		break
