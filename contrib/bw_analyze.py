#!/usr/bin/env python3

import json
import inspect
import sys
import argparse
import collections

def main():
    prog = 'bw_analyze.py'
    description = ('Script to analyze output from bw_test.')
    parser = argparse.ArgumentParser(prog=prog, description=description)
    parser.add_argument('infile', nargs='?', type=argparse.FileType(),
                        help='a JSON input file')
    options = parser.parse_args()
    
    infile = options.infile or sys.stdin

    with infile:
        try:
#           obj = json.load(infile, object_pairs_hook=collections.OrderedDict)
            obj = json.load(infile)
        except ValueError as e:
            raise SystemExit(e)


        print(obj)



if __name__ == '__main__':
    main()
