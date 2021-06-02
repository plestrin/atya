import struct
import sys

def atya_iter(stream=None):
	if stream is None:
		stream = sys.stdin.buffer
	while True:
		data = stream.read(8)
		if data:
			size = struct.unpack('<Q', data)[0]
			yield stream.read(size)
		else:
			break

def atya_write(pattern, stream=None):
	if stream is None:
		stream = sys.stdout.buffer
	stream.write(struct.pack('<Q', len(pattern)))
	stream.write(pattern)
