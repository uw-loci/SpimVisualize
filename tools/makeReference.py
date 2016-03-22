#!/usr/bin/env python

from os import listdir, rename
from os.path import isfile, join

path = 'e:/spim/phantom/'

if __name__ == '__main__':
	onlyfiles = [f for f in listdir(path) if isfile(join(path, f))]


	for f in onlyfiles:
		if '.registration.txt' in f:
			newname = f.replace('registration', 'reference')

			print(f, ' -> ',newname)

			rename(f, newname)

#