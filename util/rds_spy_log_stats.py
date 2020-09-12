#!/usr/bin/env python3

from __future__ import print_function

import csv
import glob
import subprocess

spy_logs_root = '../../rds-spy-logs'
stats_app = '../build/rdsstats'

fill_columns = True
columns = []
file_stats = {}

file_num = 0
for fname in glob.glob(spy_logs_root +'/**/*.spy'):
    print(fname)
    stats = {}
    cmd = [stats_app, fname]
    for line in subprocess.check_output(cmd).splitlines():
        items = line.decode('utf-8').split(':')
        if fill_columns:
            columns.append(items[0])
        stats[items[0]] = int(items[1])
    file_stats[fname] = stats
    fill_columns = False
    file_num += 1

with open("spy_stats.csv", "w") as f:
    writer = csv.writer(f)
    writer.writerow(['File'] + columns)
    for fname in file_stats.keys():
        stats = file_stats[fname]
        row = [stats[col] for col in columns]
        base_name = fname[len(spy_logs_root)+1:]
        writer.writerow([base_name] + row)
