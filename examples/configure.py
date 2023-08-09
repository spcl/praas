#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(prog='Docker Builder')
parser.add_argument('input')
parser.add_argument('output')
parser.add_argument('directory', type=str)
args = parser.parse_args()

with open(args.input, 'r') as in_file:

    data = in_file.read()

    formatted_data = data.format(FUNCTIONS_DIRECTORY=args.directory)

with open(args.output, 'w') as out_file:
    out_file.write(formatted_data)
