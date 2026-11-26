#!/bin/bash
# setup_goken
#	Build goken9cc (the Plan 9 compilers, hosted on Linux) and its
#	Plan 9 style libc for amd64, in $goken.  About 10 minutes.
set -euo pipefail
. "$(dirname "$0")/../common.sh"

sudo apt-get install -y --no-install-recommends bison flex gcc git ca-certificates >/dev/null

[ -d "$goken" ] || git clone --depth 1 https://github.com/aryx/goken9cc.git "$goken"
cd "$goken"
./configure >/dev/null
./scripts/build-mk.sh >/dev/null
./scripts/promote-mk.sh >/dev/null
. ./env.sh
export objtype=amd64 cputype=amd64
mk >build.log 2>&1
mk install >>build.log 2>&1
(cd lib_core/libc && mk objtype=amd64 cputype=amd64 install) >>build.log 2>&1

echo "goken ready: $goken/ROOT/arch/boot-gcc/bin/6c, $goken/ROOT/arch/amd64/lib/libc.a"
