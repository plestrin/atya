from atya import atya_iter

def yarastr(pattern):
	HEX = ('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F')
	return '{' + ' '.join([HEX[b >> 4] + HEX[b & 0xf] for b in pattern]) + '}'

for PATTERN in atya_iter():
	print('$ = %s' % yarastr(PATTERN))
