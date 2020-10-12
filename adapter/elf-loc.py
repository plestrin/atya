import sys

from elftools.elf.elffile import ELFFile

if len(sys.argv) != 2:
	sys.stderr.write('[-] usage: FILE\n')
	exit(-1)

patterns = sys.stdin.readlines()
patterns = [p.strip().decode('hex') for p in patterns]

with open(sys.argv[1], 'rb') as f:
	data = f.read()
	elf = ELFFile(f)

	for p in patterns:
		offset = 0
		found = False
		while True:
			offset = data.find(p, offset)
			if offset == -1:
				break

			for section in elf.iter_sections():
				if section.header['sh_type'] in ('SHT_NULL', 'SHT_NOBITS'):
					continue
				if section.header['sh_offset'] <= offset and section.header['sh_offset'] + section.header['sh_size'] > offset:
					sys.stdout.write('%s+%06x ' % (section.name, offset - section.header['sh_offset']))

			found = True
			offset += 1
		if not found:
			sys.stderr.write('[-] unable to find %s in %s' % (p.encode('hex'), sys.argv[1]))
		else:
			sys.stdout.write('%s\n' % (repr(p)))
