from atya import atya_iter

def getname(record, size):
	key = str(size)
	if key in record:
		record[key] += 1
	else:
		record[key] = 1
	return '$%03u_%03u' % (size, record[key])

def yarastr(pattern):
	HEX = ('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F')
	return '{' + ' '.join([HEX[b >> 4] + HEX[b & 0xf] for b in pattern]) + '}'

RECORD = {}
for PATTERN in atya_iter():
	print('%s = %s' % (getname(RECORD, len(PATTERN)), yarastr(PATTERN)))
