# data

Substitution files with other roofs; two of them are the default files of the programs.

Each is written by the kernel search with its options (`--roofs`, `--jexcess`, `--tcand`: `src/search/kernel_search.h`),
and says so in its third line, a comment (`# options: ...`) that the readers skip; read them with `-f`
(`zipper3 -f data/s3/...`, `zipper5 -f data/s4/...`).

**Two are the default files:** `s3/subst_4.00_9.50_t3_r1.txt` is the default of `zipper3`, and
`s4/subst5_2.30_4.50_t1_r0.txt` that of `zipper5`, and of the tools that take their default as the programs do (`overdraw`,
`roofs`, `strays`, `cull`, `islands5`). The programs read the default from the working folder, so run them in the folder
that holds the file, or give `-f`; when it is missing they say where it is. The files of the legacy programs are in
`legacy/s3/` and `legacy/s4/`, and are read where their pictures are meant: `zipper3 --pairs` and `link3` read
`legacy/s3/subst_4.00_9.50_t3.txt` (the linked pairs are loops of its arcs), and `film5` reads
`legacy/s4/subst5_2.30_4.50_t1.txt`.

- `s3/subst_4.00_9.50_t3_r0.txt`: `chz kernel 4 9.5 3 --roofs` (the default steep t, the roofs chosen with no excess)
- `s3/subst_4.00_9.50_t3_r1.txt` (**the default of `zipper3`**): `chz kernel 4 9.5 3 --roofs --jexcess 1` (the same, one letter of excess)
- `s3/subst_4.00_9.00_t1_r0.txt`: `chz kernel 4 9 1 --roofs` (the `_t1` search, the roofs chosen with no excess)
- `s4/subst5_2.30_4.50_t1_r0.txt` (**the default of `zipper5`**): `chz5 kernel 2.3 4.5 3 --roofs` (the default t, the roofs chosen with no excess)
- `s4/subst5_2.30_4.50_t1_r1.txt`: `chz5 kernel 2.3 4.5 3 --roofs --jexcess 1` (the same, one letter of excess)
- `s4/subst5_2.30_4.50_t1_i18_r1.txt`: `chz5 kernel 2.3 4.5 3 --tcand 18 --roofs --jexcess 1` (candidate 18 for t)

`docs/developer_guide.md` has what these roofs change in the pictures (overdraw, strays, leaves).
