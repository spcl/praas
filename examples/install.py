#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys

parser = argparse.ArgumentParser(prog='Python installer')
parser.add_argument('directory', type=str)
args = parser.parse_args()

requirements_file = os.path.join(args.directory, 'requirements.txt')
if os.path.exists(requirements_file):

    subprocess.check_call([sys.executable, '-m', 'pip', 'install', '-r' , requirements_file])

