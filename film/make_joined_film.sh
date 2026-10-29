#!/bin/sh
# make_joined_film.sh -- the whole zipper film: the 2D half (the S^2 program's film pipeline,
# with its captions off and the sphere shots at zoom 0.88), the 3D half (zipper3) and the ending
# on S^4 (film5), all without captions; then one pass that typesets every caption with LaTeX
# (film/captions.py: Computer Modern, slate #2c3e50, centred at the bottom); then the music.
#
#   film/make_joined_film.sh SCRATCH OUT.mp4 FUGUE.m4a FUGUE.wav TIMING2D.json TIMING3D.json
#
#   SCRATCH        an absolute path: every frame and every file the script makes goes there
#   OUT.mp4        the film
#   FUGUE.m4a      the recording of the fugue (BWV 891), put on the film unchanged
#   FUGUE.wav      a 16-bit PCM copy of it (afconvert -f WAVE -d LEI16), for the 2D half
#   TIMING2D.json  the timing map of bars 1-46 of the recording
#                  (legacy/s2/movie/music/align.py FIRST.wav TIMING2D.json --last-bar 46)
#   TIMING3D.json  the timing map of bars 46-101
#                  (align.py SECOND.wav TIMING3D.json --first-bar 46 --last-bar 101 --offset T0)
# and, in the environment:
#   CREDIT         a file holding the LaTeX source of the credit line (no credit without it)
#   FINAL          the time of the last chord, in seconds (make_landmarks4d.py --final; without it,
#                  bar 101 of TIMING3D.json, which align.py extrapolates through the ritardando)
#   PYTHON         a Python 3 with numpy and Pillow, for the 2D half and the captions (default python3)
#   ZIPPERS_BUILD  the CMake build folder, with the viewers (default build/)
#
# The 3D half begins where the 2D half ends (the storyboard's film_end, on TIMING2D.json); the
# ending begins at bar 96 (on TIMING3D.json).  Step 1 makes the plan from the timing maps
# (SCRATCH/script3d.txt, SCRATCH/landmarks4d.txt) before any picture is drawn.  zipper3 --movie
# and film5 open windows.
#
# Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
set -e
P="$(cd "$(dirname "$0")/.." && pwd)"
M="$P/legacy/s2/movie"
B="${ZIPPERS_BUILD:-$P/build}"     # the CMake build folder
PY="${PYTHON:-python3}"            # with numpy and Pillow
if [ $# -ne 6 ]; then
    echo "usage: film/make_joined_film.sh SCRATCH OUT.mp4 FUGUE.m4a FUGUE.wav TIMING2D.json TIMING3D.json" >&2
    exit 2
fi
S="$1"; OUT="$2"; M4A="$3"; WAV="$4"; T2="$5"; T3="$6"
case "$S" in
    /*) ;;
    *) echo "make_joined_film.sh: SCRATCH must be an absolute path, not $S" >&2; exit 2;;
esac
for f in "$M4A" "$WAV" "$T2" "$T3" ${CREDIT:+"$CREDIT"}; do
    [ -f "$f" ] || { echo "make_joined_film.sh: no such file: $f" >&2; exit 2; }
done
mkdir -p "$S"

# ---- 1. the plan: the frames of the three parts, from the timing maps ----------------------------
# the 2D half, bars 1 to 46 (beat 1.5): a copy of the S^2 program's storyboard (its own file is
# never changed) with the captions emptied and the sphere scenes at zoom 0.88
python3 - "$M/storyboard.json" "$S/storyboard_2d_nocaptions_zoom088.json" <<'EOF'
import json, sys
B = json.load(open(sys.argv[1]))
for s in B['shots']:
    s['captions'] = []
for k in ['arcs_fig8', 'ghost_arcs', 'fig8_n2_R5', 'RRL_n3', 'RLLRL_n4', 'fig8_n2_R4_wheel']:
    B['scenes'][k]['zoom'] = 0.88
json.dump(B, open(sys.argv[2], 'w'), indent=1)
EOF
# the first frame of the 3D half: make_movie.py runs the 2D half on to the storyboard's film_end
K3=$("$PY" - "$M" "$S/storyboard_2d_nocaptions_zoom088.json" "$T2" <<'EOF'
import json, os, sys
sys.path.insert(0, sys.argv[1])
import render_film as RF
B = json.load(open(sys.argv[2]))
RF.TMAP = RF.load_timing(os.path.abspath(sys.argv[3]))
print(RF.frame_at(RF.time_of(B.get('film_end') or B['shots'][-1]['end'])))
EOF
)
# the 3D half, from there to bar 96: its script, and the same without captions
python3 "$P/film/make_script.py" "$P/film/storyboard_3d.txt" "$T3" "$S/script3d.txt" --start "$K3"
awk '/^#/ {print; next} {printf "%s %s %s %s %s %s -\n", $1,$2,$3,$4,$5,$6}' "$S/script3d.txt" > "$S/script3d_nocaptions.txt"
# the ending on S^4: the last six bars (from bar 96), then the credit
python3 "$P/film/make_landmarks4d.py" "$T3" "$S/landmarks4d.txt" ${FINAL:+--final "$FINAL"}

# ---- 2. the pictures, without captions ------------------------------------------------------------
"$PY" "$M/make_movie.py" --board "$S/storyboard_2d_nocaptions_zoom088.json" --timing "$T2" --work "$S/film2d" \
    --out "$S/film2d/half2d.mp4" --contact "$S/film2d/half2d_contact.png" --audio "$WAV" -j 5 --no-check
rm -rf "$S/frames3d"; mkdir -p "$S/frames3d"
(cd "$P/legacy/s3" && "$B/legacy/s3/zipper3" -s 840 --pairs linked_pairs.txt --movie "$S/frames3d" --moviescript "$S/script3d_nocaptions.txt" --moviestart "$K3")
# the ending, drawn by apps/s4/film5 (the corrected S^4 word reader, and the leaves drawn through their tips)
rm -rf "$S/frames4d"; mkdir -p "$S/frames4d"
(cd "$P/legacy/s4" && "$B/apps/s4/film5" -f subst5_2.30_4.50_t1.txt --landmarks "$S/landmarks4d.txt" --pens 19,42 --nocaptions --movie "$S/frames4d")

# ---- 3. the captions, typeset with LaTeX, on every frame ------------------------------------------
rm -rf "$S/joined"
"$PY" "$P/film/captions.py" --frames "$S/film2d/final" "$S/frames3d" "$S/frames4d" --timing2d "$T2" \
    --script3d "$S/script3d.txt" --landmarks4d "$S/landmarks4d.txt" ${CREDIT:+--credit "$CREDIT"} \
    --cache "$S/captions_cache" --out "$S/joined" --jobs 5

# ---- 4. encode, and add the music (passed through, unchanged) -------------------------------------
"$B/film/swift/encode_movie" "$S/joined" "$S/joined_video.mp4" 30 8
"$B/film/swift/add_audio" "$S/joined_video.mp4" "$M4A" "$OUT" 0 -1
echo "wrote $OUT"
