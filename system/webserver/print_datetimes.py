#!/usr/bin/env python3
import os
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.tools.lib.route import SegmentName
from openpilot.tools.lib.logreader import LogReader
from datetime import datetime, timedelta, timezone

BRT = timezone(timedelta(hours=-3))

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
      print(f'stopped in {segment_path} and segment {i}', flush=True)
      break
    try:
      rlogs = LogReader(segment_path)
      if any(msg.which() == "userBookmark" for msg in rlogs):
        segments_with_bookmark.append(i)
        print(f'\nbookMark found in: {segment_path} and segment: {i}', flush=True)
        end = datetime.now()
        print(f'Executed in: {end - begin}\n', flush=True)
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
    wall_time_rlog = 1
    should_minus_one_minute = False
    rlogs = None
    try:
      rlogs = LogReader(os.path.join(Paths.log_root(), unique_route + "--1/rlog.zst"))
      should_minus_one_minute = True
    except AssertionError:
      try:
        rlogs = LogReader(os.path.join(Paths.log_root(), unique_route + "--0/rlog.zst"))
      except AssertionError:
        print(f'Route {unique_route} in process of deleting')
    if rlogs is not None:
      wall_time_rlog = next(
        (rlog.gpsLocationExternal.unixTimestampMillis
          for rlog in rlogs
          if rlog.which() == "gpsLocationExternal"),
        1
      )
    temp_date_time = datetime.fromtimestamp(wall_time_rlog / 1e3, tz=timezone.utc)
    temp_date_time = temp_date_time.astimezone(BRT)
    temp_date_time = temp_date_time - timedelta(minutes=1) if should_minus_one_minute else temp_date_time
    date_time.append(temp_date_time.strftime("%Y-%m-%d  %H:%M"))
    should_minus_one_minute = False
  return unique_routes, date_time

if __name__ == "__main__":
  begin = datetime.now()
  unique_routes, b = all_routes()
  for i, (_, __) in enumerate(zip(unique_routes, b, strict=True)):
    print(f'i:{i+1:03d}  {_}  {__}', flush=True)
  end = datetime.now()
  print(f'Executed in: {end - begin} \n', flush=True)

  begin = datetime.now()
  userbookmark_segments_list = [get_userbookmark_segments(r) for r in sorted(unique_routes, reverse=True)]
  end = datetime.now()
  print(f'Executed in: {end - begin}')
