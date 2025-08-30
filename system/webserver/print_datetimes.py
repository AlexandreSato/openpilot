#!/usr/bin/env python3

import os
import warnings
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.tools.lib.route import SegmentName
from openpilot.tools.lib.logreader import LogReader
from datetime import datetime

warnings.filterwarnings("ignore", category=RuntimeWarning, message="Corrupted events detected")

def segment_to_segment_name(data_dir, segment):
    fake_dongle = "ffffffffffffffff"
    return SegmentName(str(os.path.join(data_dir, fake_dongle + "|" + segment)))

def all_segment_names():
    segments = []
    for segment in listdir_by_creation(Paths.log_root()):
        try:
            segments.append(segment_to_segment_name(Paths.log_root(), segment))
        except AssertionError:
            pass
    return segments

def get_userbookmark_segments(unique_route):
    segments_with_bookmark = []
    i = 0
    while True:
        segment_path = os.path.join(Paths.log_root(), f"{unique_route}--{i}/rlog.zst")
        if not os.path.exists(segment_path):
            break
        try:
            rlogs = LogReader(segment_path)
            if any(msg.which() == "userBookmark" for msg in rlogs):
                segments_with_bookmark.append(i)
        except AssertionError:
            pass
        i += 1
    return segments_with_bookmark

def all_routes():
    segment_names = all_segment_names()
    route_names = [segment_name.route_name for segment_name in segment_names]
    route_times = [route_name.time_str for route_name in route_names]
    unique_routes = list(dict.fromkeys(route_times))
    date_time = []
    for unique_route in unique_routes:
        wall_time_rlog = None
        try:
            rlogs = LogReader(os.path.join(Paths.log_root(), unique_route + "--1/rlog.zst"))
        except AssertionError:
            rlogs = LogReader(os.path.join(Paths.log_root(), unique_route + "--0/rlog.zst"))
        wall_time_rlog = next(
            (rlog.gpsLocationExternal.unixTimestampMillis
                for rlog in rlogs
                if rlog.which() == "gpsLocationExternal"),
            None
        )
        date_time.append(datetime.fromtimestamp(wall_time_rlog / 1e3))
    # userbookmark_segments_list = [get_userbookmark_segments(r) for r in unique_routes]
    # return unique_routes, date_time, userbookmark_segments_list
    return unique_routes, date_time

a, b = all_routes()
for _, __ in zip(a, b, strict=True):
    print(f'a: {_}     b: {__}')
