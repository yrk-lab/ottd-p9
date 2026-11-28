# Phase 6: the source bundle (issue #34)

The Plan 9 community expects sources, even generated ones: a binary-only distribution suits a few, and the source
leaves room to build for other platforms and to experiment. `srcbundle.sh` makes
`work/openttd-<version>-src.tgz`: OpenTTD as C that Plan 9's own compilers build, on 9front or 9legacy, with
nothing from the host.

	./srcbundle.sh

## What is in it

- `tu/`: the C the pipeline generated (../phase2/work/tu: the units the mkfile names, `invoke.c`, `ctors.c`), the
  generated mkfile and rules, and `config.9front` and `config.9legacy` with the bundle's paths (`P0=../runtime`,
  FreeType's library in `../freetype`).
- `runtime/`: the hand-written runtime, the parts the two configurations name (`../phase0/plan9`, with its headers
  and the cbe shim, and `../phase0/eh`).
- `freetype/`: FreeType's source (2.15.0), with its own Plan 9 mkfile.
- The data, as the binary bundle's; `doc/`: the licences, OpenTTD's and its third-party code's, and those of what
  was translated with it (libc++, libc++abi, zlib, liblzma) or is linked (FreeType).
- `mkfile` (`srcbundle.mkfile`): `mk` builds FreeType, then the game (`tu`, with the system's configuration: 9front
  where libc.h has atomic(2), else 9legacy), and leaves `6.openttd` next to the data; `mk install` as the binary
  bundle's (BIN as usual).
- `README.Plan9` (`srcbundle.README`): that the C is generated, by an automated pipeline, not written by hand, and
  how, step by step; which files are generated (don't edit: translate again) and which are hand-written (the
  runtime); the provenance, filled in by srcbundle.sh: this repository's commit, OpenTTD's version and hash,
  clang's, LLVM's and llvm-cbe's versions, the FreeType, zlib and xz tags.

## Checked

`srctest.rc` unpacks the bundle in the VM, runs `mk`, plays the gate's scenarios with the built `6.openttd`, and
installs it with `mk install` in a fresh home:

	script=phase6/srctest.rc ../phase2/run9front.sh
	script=phase6/srctest.rc ../phase5/run9legacy.sh

Its saves, normalized, against the reference build's (`../phase1/gate.sh cmp ref src9front`):

- 9front (#35): built from source in about 42 minutes (647 compiler commands), the gate the same, installed and run.
- 9legacy (#36): built from source with 9legacy's own compilers (640 commands), the gate the same, installed and run.
