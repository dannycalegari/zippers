# data

The data files of the programs: the substitution files of the S³ and S⁴ examples, and the file of linked pairs. An
install puts them in `share/zippers/` (`s3/`, `s4/`).

Each substitution file is written by the kernel search (`zippers search`, or `chz kernel` and `chz5 kernel`); a file
written with options (`--roofs`, `--jexcess`, `--tcand`: `src/search/kernel_search.h`) says so in its third line, a
comment (`# options: ...`) that the readers skip.

**How the programs find them.** `zipper3`, `zipper5`, `link3`, `film5`, the tools (`overdraw`, `roofs`, `strays`, `cull`,
`islands5`) and `zippers` look for a data file in this order (`src/core/data_files.h`): a name that contains `/` as
given; the working folder; the folders of the environment variable `ZIPPERS_DATA` (separated by `:`; in each,
`s3/NAME` or `s4/NAME`, then `NAME`); then this folder, `data/` (for the programs of a build of this source tree), or
`share/zippers/` (for installed programs). The first place that holds an entry of the name is the one used (an entry
that is not a readable file, such as a folder of that name, stops the program, which says what is there). So `zipper3
-f subst_4.00_9.50_t3_r0.txt` reads the file of this folder from any folder that holds nothing of that name; when a file
is found nowhere, the program says where it looked.
`zippers list EXAMPLE` shows where each default file is found.

**The default files:**
- `s3/subst_4.00_9.50_t3_r1.txt`: of `zipper3`, and of the tools on S³;
- `s3/subst_4.00_9.50_t3.txt`: of `link3` and `zipper3 --pairs` (the linked pairs are loops of its base arcs: with
  other roofs they are other curves);
- `s3/linked_pairs.txt`: the 13 linked pairs (`zipper3 --pairs linked_pairs.txt`), written by `link3 --export` from the
  scan of `link3 -a 0.01 --nrel 20 --hr 2.6`;
- `s4/subst5_2.30_4.50_t1_r0.txt`: of `zipper5`, and of the tools on S⁴;
- `s4/subst5_2.30_4.50_t1.txt`: of `film5` (the film's ending).

`s3/subst_4.00_9.50_t3.txt`, `s3/linked_pairs.txt` and `s4/subst5_2.30_4.50_t1.txt` are copies of the files of the
legacy programs, in `legacy/s3/` and `legacy/s4/`, byte for byte (the smoke test checks it), so that every default file
is in this folder.

**All the files, and the searches that made them:**
- `s3/subst_4.00_9.50_t3.txt`: `chz kernel 4 9.5 3` (the default steep t)
- `s3/subst_4.00_9.50_t3_r0.txt`: `chz kernel 4 9.5 3 --roofs` (the same t, the roofs chosen with no excess)
- `s3/subst_4.00_9.50_t3_r1.txt` (**the default of `zipper3`**): `chz kernel 4 9.5 3 --roofs --jexcess 1` (the same,
  one letter of excess)
- `s3/subst_4.00_9.00_t1_r0.txt`: `chz kernel 4 9 1 --roofs` (the `_t1` search, the roofs chosen with no excess)
- `s4/subst5_2.30_4.50_t1.txt`: `chz5 kernel 2.3 4.5 3` (the default t)
- `s4/subst5_2.30_4.50_t1_r0.txt` (**the default of `zipper5`**): `chz5 kernel 2.3 4.5 3 --roofs` (the default t, the
  roofs chosen with no excess)
- `s4/subst5_2.30_4.50_t1_r1.txt`: `chz5 kernel 2.3 4.5 3 --roofs --jexcess 1` (the same, one letter of excess)
- `s4/subst5_2.30_4.50_t1_i18_r1.txt`: `chz5 kernel 2.3 4.5 3 --tcand 18 --roofs --jexcess 1` (candidate 18 for t)

The other files of the legacy programs (`subst_4.00_9.00_t1.txt`, `subst_4.00_9.00_t2.txt`, `subst5_2.30_4.50_t2.txt`,
`subst5_2.30_4.50_t3.txt`) are in `legacy/s3/` and `legacy/s4/`; give their paths with `-f`.

`docs/developer_guide.md` has what these roofs change in the pictures (overdraw, strays, leaves).
