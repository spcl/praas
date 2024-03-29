#!/usr/bin/env python

import argparse

import docker

BENCHMARKS = [
    'hello-world'
]

parser = argparse.ArgumentParser(prog='Docker Builder')
parser.add_argument('example', choices=BENCHMARKS)
parser.add_argument('language', choices=['cpp', 'python'])
parser.add_argument('-v', '--verbose', action='store_true')

args = parser.parse_args()
client = docker.from_env()

try:
    image, output = client.images.build(
        path='examples',
        tag=f'spcleth/praas-examples:{args.example}-{args.language}',
        dockerfile=f'Dockerfile.{args.language}',
        buildargs={
            'example': f'{args.example}-{args.language}'
        }
    )
    print(f'Built image {image.tags}')
    if args.verbose:
        print("Build output")
        for line in output:
            if 'stream' in line:
                print(line['stream'], end='')
except docker.errors.BuildError as e:
    print(e)
