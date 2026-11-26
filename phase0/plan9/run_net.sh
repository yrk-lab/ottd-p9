#!/bin/bash
# run_net [test.c...]
#	Fast loop for the sockets (net_p9.c): each test in tests_net/,
#	built natively with gcc, and again with its socket calls
#	renamed c_<name> (as lower.py --libc renames them) and linked
#	with net_p9.c and netp9p.c through plan9port (9c, 9l): the
#	same output means net_p9.c behaves as Linux does.  9front runs
#	the same tests translated, with net9f.c (phase2/run9front.sh).
#	Needs plan9port in $PLAN9 (default ~/plan9).
set -uo pipefail
. "$(dirname "$0")/../common.sh"
export PLAN9=${PLAN9:-$HOME/plan9}
[ -x "$PLAN9/bin/9c" ] || fatal "no plan9port in $PLAN9"
plan9=$top/plan9
d=$out/net
mkdir -p "$d"

# the socket calls lower.py renames
names="socket bind listen accept connect send sendto recv recvfrom setsockopt getsockopt
getsockname getpeername getaddrinfo freeaddrinfo getnameinfo gai_strerror getifaddrs freeifaddrs
select ioctl close"
for n in $names; do
	echo "#define $n c_$n"
done >"$d/rename.h"

"$PLAN9/bin/9c" -o "$d/net_p9.o" "$plan9/net_p9.c" &&
"$PLAN9/bin/9c" -o "$d/netp9p.o" "$plan9/netp9p.c" || fatal "9c failed"

pass=0
n=0
[ $# -gt 0 ] || set -- "$top"/tests_net/*.c
for f in "$@"; do
	name=$(basename "${f%.c}")
	n=$((n+1))
	gcc -O1 -U_FORTIFY_SOURCE -o "$d/$name.native" "$f" &&
	gcc -O1 -U_FORTIFY_SOURCE -include "$d/rename.h" -c -o "$d/$name.o" "$f" &&
	"$PLAN9/bin/9l" -o "$d/$name.p9p" "$d/$name.o" "$d/net_p9.o" "$d/netp9p.o" -lip -lpthread ||
		{ echo "$name: BUILD FAILED"; continue; }
	timeout 60 "$d/$name.native" >"$d/$name.expected" 2>&1
	timeout 60 "$d/$name.p9p" >"$d/$name.out" 2>&1
	if cmp -s "$d/$name.expected" "$d/$name.out"; then
		echo "$name: PASS"
		pass=$((pass+1))
	else
		echo "$name: DIFFERENT"
		diff "$d/$name.expected" "$d/$name.out" | sed 20q
	fi
done
echo "net tests: $pass/$n"
[ $pass -eq $n ]
