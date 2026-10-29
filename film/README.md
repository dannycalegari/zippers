# film

The tools that make the joined film of the zippers: S², S³ and S⁴, set to J. S. Bach's Fugue in B♭ minor from *The
Well-Tempered Clavier*, Book II (BWV 891).

- `swift/`: the five Swift tools, one copy for both films: `encode_movie` (PNG frames to MP4), `add_audio` (put a
  soundtrack on a film), `probe_movie`, `grab_frames`, `extract_audio`. They are built on macOS (option
  `ZIPPERS_FILM`) into `build/film/swift/`; the S² film pipeline finds them through `legacy/s2/movie/bin`.
- `make_joined_film.sh` makes the film:
  - the 2D half with the S² program's film pipeline (`legacy/s2/movie/make_movie.py`), its captions off and the sphere
    shots at zoom 0.88;
  - the 3D half with `build/legacy/s3/zipper3 --movie`, run inside `legacy/s3`;
  - the ending on S⁴ with `build/apps/s4/film5`, run inside `legacy/s4`;
  - every caption with `captions.py`, then the soundtrack with the Swift tools.

  It needs the viewer build (`zipper3 --movie` and `film5` open windows); set `ZIPPERS_BUILD` for another build
  folder.
- `storyboard_3d.txt`: the segments of the 3D half, keyed to bars and beats of the score. `make_script.py` turns it
  into a `zipper3 --moviescript` file (frame counts) on a timing map.
- `make_landmarks4d.py`: the landmarks of the ending (its last six bars), on the timing map of bars 46–101, for `film5`
  and `captions.py`.
- `captions.py`: the captions of the whole film, typeset with LaTeX (Computer Modern, slate #2c3e50, centred at the
  bottom, on a soft white panel) and laid on caption-free frames. It needs `pdflatex` and Ghostscript.

No music and no recordings are kept here: the scripts take them as arguments.

## Making the film from a recording

Every picture is keyed to the score, so a recording enters only through two timing maps, written by
`legacy/s2/movie/music/align.py`. It aligns the Humdrum score `wtc2f22.krn` (David Huron, in
[github.com/humdrum-tools/bach-wtc](https://github.com/humdrum-tools/bach-wtc)) to the recording; its rights are
reserved, so it is not in this repository, and `align.py` reads it from `legacy/s2/movie/music/`. With `FUGUE.m4a` the
recording:

1. A 16-bit PCM copy: `afconvert -f WAVE -d LEI16 FUGUE.m4a FUGUE.wav`.
2. The timing map of bars 1–46, from an excerpt that starts at the first note:
   `python3 legacy/s2/movie/music/align.py FIRST.wav TIMING2D.json --last-bar 46`. It also times the named
   landmarks of `legacy/s2/movie/music/landmarks.json`, among them `C2next`, where the 2D half ends.
3. The timing map of bars 46–101, from an excerpt that starts a little before bar 46, T0 seconds into the recording,
   and ends just after the last chord: `align.py SECOND.wav TIMING3D.json --first-bar 46 --last-bar 101 --offset T0`.
   Cut the silence after the last chord first: with several seconds of it, the open-end alignment stretches the last
   bars.
4. The attack of the last chord, measured on the waveform: `align.py` extrapolates bar 101 through the ritardando.
5. Then, with `SCRATCH` an absolute path and `credit.tex` a file holding the credit line in LaTeX:

       CREDIT=credit.tex FINAL=<the attack, in seconds> PYTHON=<a Python 3 with numpy and Pillow> \
           film/make_joined_film.sh SCRATCH OUT.mp4 FUGUE.m4a FUGUE.wav TIMING2D.json TIMING3D.json

   The 3D half begins where the 2D half ends (the storyboard's `film_end`, on `TIMING2D.json`) and the ending at bar
   96 (on `TIMING3D.json`). The script makes this plan first (`SCRATCH/script3d.txt`, `SCRATCH/landmarks4d.txt`, and
   the tables that `make_script.py` and `make_landmarks4d.py` print), then the pictures. Every file it makes goes into
   `SCRATCH`.
