#!/bin/bash
# The encoding test: 60 frames of shots/rot_n2.txt, encoded, with 2 s of the music added in
# the three ways add_audio allows; each result is probed.  Everything goes to the scratch dir.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; BIN="$HERE/../bin"
SCR="${SCRATCH:?set SCRATCH to a scratch folder (the frames and the audio go there)}"
WAV="${WAV:?set WAV to a 16-bit PCM WAV of the music (2 s of it are used)}"
W="$SCR/encode_test"; rm -rf "$W"; mkdir -p "$W"
"$BIN/zmovie" "$HERE/shots/rot_n2.txt" frames=60 -o "$W/frames" -j 4 --quiet
"$BIN/encode_movie" "$W/frames" "$W/video.mp4" 30 5
"$BIN/probe_movie" "$W/video.mp4"
echo "== AAC, pass-through (the recommended route)"
afconvert -f m4af -d aac -b 256000 "$WAV" "$W/music.m4a"
"$BIN/add_audio" "$W/video.mp4" "$W/music.m4a" "$W/aac_pass.mp4" 0 -1
"$BIN/probe_movie" "$W/aac_pass.mp4"
echo "== WAV, pass-through (LPCM in MP4)"
"$BIN/add_audio" "$W/video.mp4" "$WAV" "$W/wav_pass.mp4" 0 -1
"$BIN/probe_movie" "$W/wav_pass.mp4"
echo "== WAV, re-encoded (FADE = 0)"
"$BIN/add_audio" "$W/video.mp4" "$WAV" "$W/wav_reenc.mp4" 0 0
"$BIN/probe_movie" "$W/wav_reenc.mp4"
