#!/bin/bash
# Build goken9cc (Plan 9 compilers hosted on Linux) + its Plan 9-style libc. ~10 min. Needs bison/flex, gcc, git.
set -euo pipefail; HERE=$(cd "$(dirname "$0")" && pwd); . "$HERE/../common.sh"
G=${G:-$W/goken9cc}
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO=sudo
$SUDO apt-get install -y --no-install-recommends bison flex gcc git ca-certificates >/dev/null
[ -d "$G" ] || git clone --depth 1 https://github.com/aryx/goken9cc.git "$G"
cd "$G"; ./configure >/dev/null; ./scripts/build-mk.sh >/dev/null; ./scripts/promote-mk.sh >/dev/null
. ./env.sh; export objtype=amd64 cputype=amd64
mk >build.log 2>&1; mk install >>build.log 2>&1
( cd lib_core/libc && mk objtype=amd64 cputype=amd64 install >>"$G/build.log" 2>&1 )
echo "goken ok: $G/ROOT/arch/boot-gcc/bin/6c ; libc: $G/ROOT/arch/amd64/lib/libc.a"
