#!/usr/bin/env python3
# ir compile_commands.json outdir [jobs]
#	Recompile every translation unit of the build to LLVM bitcode,
#	outdir/<source path>.bc, with the translator's flags:
#	the build's own defines, includes and language flags, but
#	-O1 -g0 (as the rest of the pipeline), no warnings, no _FORTIFY_SOURCE
#	(glibc's __*_chk variants), no precompiled header (its header is
#	included instead), no __int128 (Plan 9 compilers have none: libc++
#	and fmt are configured without it), long double as double
#	(-mlong-double-64; no x87 80-bit type on Plan 9), not position
#	independent (-fno-pic -fno-pie: for PIC, LLVM makes relative lookup
#	tables, which are no C constants), fmt printing to a FILE through
#	fwrite and putc (FMT_USE_FALLBACK_FILE: its glibc path works on the
#	FILE's internal buffer pointers, which the Plan 9 runtime does not
#	keep), and a fixed target.
#	Units already compiled with the same command and newer than their
#	source are skipped (the command is kept in outdir/<path>.cmd), unless
#	the translator's flags changed (outdir/flags.txt).
#	Only the openttd executable's units: not the unit tests
#	(openttd_test, which has its own main).  The list of units is
#	written to outdir/units.txt.
import hashlib
import json
import os
import shutil
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

extra = ["--target=x86_64-unknown-linux-gnu", "-O1", "-g0", "-w", "-mlong-double-64", "-fno-pic", "-fno-pie",
	"-D_LIBCPP_DISABLE_EXTERN_TEMPLATE", "-D_LIBCPP_HAS_NO_INT128", "-DFMT_USE_INT128=0",
	"-DFMT_USE_FALLBACK_FILE=1", "-U_FORTIFY_SOURCE"]

# a copy of a precompiled header's header, away from its .pch: clang uses
# "x.hxx.pch" automatically when told to -include "x.hxx"
def header(h, outdir):
	d = os.path.join(outdir, "pch", hashlib.md5(h.encode()).hexdigest()[:8])
	os.makedirs(d, exist_ok=True)
	c = os.path.join(d, os.path.basename(h))
	shutil.copyfile(h, c)
	return c

def convert(entry, out, outdir):
	a = shlex.split(entry["command"])
	cmd = [a[0]]
	i = 1
	while i < len(a):
		x = a[i]
		if x == "-Xclang" and i + 3 < len(a) and a[i+1] == "-include-pch":
			i += 4		# -Xclang -include-pch -Xclang file.pch
			continue
		if x == "-Xclang" and i + 3 < len(a) and a[i+1] == "-include":
			cmd += ["-include", header(a[i+3], outdir)]
			i += 4
			continue
		if x == "-o" or x == "-c":
			i += 2 if x == "-o" else 1
			continue
		if x.startswith("-O") or x.startswith("-W") or x.startswith("-D_FORTIFY_SOURCE") or x.startswith("-flto"):
			i += 1
			continue
		if x == entry["file"]:
			i += 1
			continue
		cmd.append(x)
		i += 1
	return cmd + extra + ["-c", "-emit-llvm", "-o", out, entry["file"]]

def main():
	if len(sys.argv) < 3:
		sys.exit("usage: ir compile_commands.json outdir [jobs]")
	entries = json.load(open(sys.argv[1]))
	outdir = sys.argv[2]
	jobs = int(sys.argv[3]) if len(sys.argv) > 3 else os.cpu_count()
	os.makedirs(outdir, exist_ok=True)
	flagfile = os.path.join(outdir, "flags.txt")
	same = os.path.exists(flagfile) and open(flagfile).read() == " ".join(extra)
	work = []
	units = []
	for e in entries:
		f = e["file"]
		if f.endswith(".hxx.cxx"):	# the precompiled header itself
			continue
		if "/openttd_test.dir/" in e["command"]:
			continue
		rel = f.split("/src/", 1)[-1] if "/src/" in f else os.path.basename(f)
		out = os.path.join(outdir, rel + ".bc")
		units.append(out)
		os.makedirs(os.path.dirname(out), exist_ok=True)
		cmd = convert(e, out, outdir)	# here: header() copies files, not in parallel
		cmdfile = out[:-3] + ".cmd"
		text = " ".join(cmd)
		if same and os.path.exists(out) and os.path.getmtime(out) > os.path.getmtime(f) and \
		   os.path.exists(cmdfile) and open(cmdfile).read() == text:
			continue
		work.append((e, cmd, cmdfile, text))

	def run(job):
		e, cmd, cmdfile, text = job
		p = subprocess.run(cmd, cwd=e["directory"], capture_output=True, text=True)
		if p.returncode == 0:
			open(cmdfile, "w").write(text)
		return e["file"], p.returncode, p.stderr

	failed = 0
	with ThreadPoolExecutor(jobs) as pool:
		for f, rc, err in pool.map(run, work):
			if rc != 0:
				failed += 1
				print("%s: FAILED\n%s" % (f, "\n".join(err.splitlines()[:5])))
	if failed == 0:
		open(flagfile, "w").write(" ".join(extra))
	open(os.path.join(outdir, "units.txt"), "w").write("".join(u + "\n" for u in units))
	print("ir: %d units, %d compiled now, %d failed" % (len(units), len(work), failed))
	sys.exit(1 if failed else 0)

main()
