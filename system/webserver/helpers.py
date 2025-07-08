import os
import subprocess
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.tools.lib.route import SegmentName

def is_valid_segment(segment):
  try:
    segment_to_segment_name(Paths.log_root(), segment)
    return True
  except AssertionError:
    return False

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

def all_routes():
  segment_names = all_segment_names()
  route_names = [segment_name.route_name for segment_name in segment_names]
  route_times = [route_name.time_str for route_name in route_names]
  unique_routes = list(dict.fromkeys(route_times))
  return sorted(unique_routes, reverse=True)

def segments_in_route(route):
  segment_names = [segment_name for segment_name in all_segment_names() if segment_name.time_str == route]
  segments = [segment_name.time_str + "--" + str(segment_name.segment_num) for segment_name in segment_names]
  return segments

def ffmpeg_mp4_concat_wrap_process_builder(file_list, cameratype, chunk_size=1024*512):
  command_line = ["ffmpeg"]
  if not cameratype == "qcamera":
    command_line += ["-f", "hevc"]
    command_line += ["-vtag", "hvc1"]
  command_line += ["-r", "20"]
  command_line += ["-i", "concat:" + file_list]
  command_line += ["-c", "copy"]
  if not cameratype == "qcamera":
    command_line += ["-map", "0"]
  else:
    command_line += ["-map", "0:v?", "-map", "0:a?"]
    command_line += ["-c:a", "aac", "-ar", "44100"] # Convert audio to AAC if exists
  command_line += ["-f", "mp4"]
  command_line += ["-movflags", "empty_moov"]
  command_line += ["-"]
  return subprocess.Popen(
    command_line, stdout=subprocess.PIPE,
    bufsize=chunk_size
  )

def ffmpeg_mp4_wrap_process_builder(filename):
  """Returns a process that will wrap the given filename
     inside an mp4 container, for easier playback by browsers
     and other devices. Primary use case is streaming segment videos
     to the vidserver tool.
     filename is expected to be a pathname to one of the following
       /path/to/a/qcamera.ts
       /path/to/a/dcamera.hevc
       /path/to/a/ecamera.hevc
       /path/to/a/fcamera.hevc
  """
  basename = filename.rsplit("/")[-1]
  extension = basename.rsplit(".")[-1]
  command_line = ["ffmpeg", "-flags", "low_delay", "-i", filename]

  if extension == "ts":
    # For TS files (qcamera) containing video + audio
    command_line += ["-map", "0:v?"]  # video stream
    command_line += ["-map", "0:a?"]  # audio stream
    command_line += ["-c:a", "aac", "-ar", "44100", "-dn"]
  elif extension == "hevc":
    # For HEVC files (no audio)
    command_line += ["-f", "hevc"]
    command_line += ["-map", "0"]    # map all streams (only video in this case)
    command_line += ["-vtag", "hvc1"] # required for HEVC in MP4

  command_line += ["-c:v", "copy"] # copy video without re-encoding
  command_line += ["-r", "20"]       # force 20 FPS (if needed)
  command_line += ["-f", "mp4"]      # output format
  command_line += ["-movflags", "frag_keyframe+empty_moov+faststart"] # allows streaming without file completion
  command_line += ["-"]              # output to stdout

  return subprocess.Popen(
    command_line, stdout=subprocess.PIPE
  )
