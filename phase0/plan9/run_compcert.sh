#!/bin/bash
# run_compcert [prog...]
#	The CompCert benchmarks from goken9cc (benchs/compcert: ISO C
#	with reference outputs) through p9check.  Each program's output
#	must equal both the native build and Results/prog.
set -uo pipefail
. "$(dirname "$0")/../common.sh"

dir=$goken/benchs/compcert
progs=${*:-"fib integr qsort fft fftsp fftw sha1 sha3 aes almabench lists binarytrees
	fannkuch knucleotide mandelbrot nbody nsieve nsievebits spectral vmach bisect chomp
	perlin siphash24"}

pass=0
n=0
for p in $progs; do
	x=
	case $p in
	siphash24)	# goken edited it to get u64 etc. from its own u.h
		x="-include stdint.h -Du64=uint64_t -Du32=uint32_t -Du8=uint8_t" ;;
	esac
	n=$((n+1))
	r=$("$top/plan9/p9check.sh" -x "$x" -e "$dir/Results/$p" "$dir/$p.c")
	echo "$r"
	case $r in
	*": PASS")	pass=$((pass+1)) ;;
	esac
done
echo "compcert: $pass/$n"
[ $pass -eq $n ]
