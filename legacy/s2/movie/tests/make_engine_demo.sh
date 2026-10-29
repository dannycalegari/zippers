#!/bin/bash
# The engine demo: render the shot, caption it, encode it, add 6 s of the music.
# Frames and audio intermediates go to the scratch directory (SCRATCH).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/../bin"; SRC="$HERE/../src"
SCR="${SCRATCH:?set SCRATCH to a scratch folder (the frames and the audio go there)}"
WAV="${WAV:?set WAV to a 16-bit PCM WAV of the music (6 s of it are used)}"
W="$SCR/engine_demo"; rm -rf "$W"; mkdir -p "$W"
"$BIN/zmovie" "$HERE/shots/engine_demo.txt" -o "$W/raw" -j 6
python3 "$SRC/caption.py" "$W/raw" "$W/cap" "$HERE/engine_demo_captions.txt"
"$BIN/encode_movie" "$W/cap" "$W/video.mp4" 30 5
# AAC first (LPCM in an MP4 is non-standard); then the audio is passed through untouched
python3 "$SRC/cut_audio.py" "$WAV" "$W/music.wav" 0 6.1 0 0.7     # (0.1 s spare: see the README)
afconvert -f m4af -d aac -b 256000 "$W/music.wav" "$W/music.m4a"
"$BIN/add_audio" "$W/video.mp4" "$W/music.m4a" "$HERE/engine_demo.mp4" 0 -1
"$BIN/probe_movie" "$HERE/engine_demo.mp4"
