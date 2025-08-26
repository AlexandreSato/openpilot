#!/usr/bin/env python3

from openpilot.tools.lib.logreader import LogReader
from datetime import datetime
import os

path = '/home/sato/commalogs/realdata/0000065b--718cea3a54--1'
qlog_path = os.path.join(path, 'qlog.zst')
rlog_path = os.path.join(path, 'rlog.zst')

qlogs = LogReader(qlog_path)
rlogs = LogReader(rlog_path)
wall_time_qlog = 0
wall_time_rlog = 0

for qlog in qlogs:
  if qlog.which() == 'initData':
    wall_time_qlog = qlog.initData.wallTimeNanos
    break

for rlog in rlogs:
  if rlog.which() == "gpsLocationExternal":
    wall_time_rlog = rlog.gpsLocationExternal.unixTimestampMillis
    break

print(f'path: {path} |  qlog: {datetime.fromtimestamp(wall_time_qlog / 1e9)}  | rlog: {datetime.fromtimestamp(wall_time_rlog / 1e3)}')
