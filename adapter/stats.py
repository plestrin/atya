from atya import atya_iter

NB_PATTERN = 0
MIN_PATTERN_LEN = 0xffffffff
MAX_PATTERN_LEN = 0

for PATTERN in atya_iter():
	NB_PATTERN += 1
	if len(PATTERN) > MAX_PATTERN_LEN:
		MAX_PATTERN_LEN = len(PATTERN)
	if len(PATTERN) < MIN_PATTERN_LEN:
		MIN_PATTERN_LEN = len(PATTERN)

print('NB pattern: %u' % NB_PATTERN)
if NB_PATTERN:
	print('Min length: %u' % MIN_PATTERN_LEN)
	print('Max length: %u' % MAX_PATTERN_LEN)
