#!/bin/bash
# CompCert benchmark corpus (from goken9cc's benchs/compcert, ISO C with reference outputs) through the C pipeline:
# each program's 6c-built output must equal both the native clang build and Results/<prog>.
P=$(cd "$(dirname "$0")" && pwd); . "$P/../common.sh"; G=${G:-$W/goken9cc}; C=$G/benchs/compcert
PROGS=${*:-"fib integr qsort fft fftsp fftw sha1 sha3 aes almabench lists binarytrees fannkuch knucleotide
  mandelbrot nbody nsieve nsievebits spectral vmach bisect chomp perlin siphash24"}
pass=0; n=0
for p in $PROGS; do
  X=""; [ $p = siphash24 ] && X="-include stdint.h -Du64=uint64_t -Du32=uint32_t -Du8=uint8_t"  # goken edited it to rely on its u.h
  n=$((n+1)); r=$(XCFLAGS="$X" bash "$P/p9check_c.sh" "$C/$p.c" "$C/Results/$p" 2>&1 | grep -v '^warning'); echo "$r"
  case $r in *": PASS"*) pass=$((pass+1));; esac
done
echo "COMPCERT: $pass/$n"
