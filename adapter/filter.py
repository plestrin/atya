import os
import sys

from atya import atya_iter, atya_write

MAX = None
MIN = None

next_is_min = False
next_is_max = False
for arg in sys.argv[1:]:
	if next_is_max:
		MAX = int(arg)
		next_is_max = False
	elif next_is_min:
		MIN = int(arg)
		next_is_min = False
	elif arg in ('-h', '--help'):
		print('usage %s [(-h | --help)] [--min LENGTH] [--max LENGTH]' % os.path.basename(sys.argv[0]))
		exit(0)
	elif arg == '--max':
		next_is_max = True
	elif arg == '--min':
		next_is_min = True

for PATTERN in atya_iter():
	if MIN and len(PATTERN) < MIN:
		continue
	if MAX and len(PATTERN) > MAX:
		continue
	atya_write(PATTERN)
