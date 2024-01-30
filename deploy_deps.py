#!/usr/bin/python3

import os
import sys

folder = "deps"
script = os.path.join(folder, "bootstrap.py")
json = os.path.join(folder, "bootstrap.json")

os.system('"{}" {} -b {} --bootstrap-file={} --break-on-first-error'.format(sys.executable, script, folder, json))
