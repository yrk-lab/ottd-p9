#!/bin/bash
# One-time: 9front amd64 root tree + kernel + u9fs for plan9/run_9front.sh (diskless boot, root served from the host).
# Needs sudo (apt, loop-mounting the ISO). Result in ${NINE:-~/9front}: root/ 9pc64 u9fs
set -euo pipefail; N=${NINE:-$HOME/9front}; REL=${REL:-11952}; mkdir -p "$N"; cd "$N"
sudo apt-get install -y --no-install-recommends qemu-system-x86 gcc git curl >/dev/null
[ -f 9front.iso ] || { curl -fL -o 9front.iso.gz "https://9front.org/iso/9front-$REL.amd64.iso.gz"; gunzip -f 9front.iso.gz; }
if [ ! -d root/rc ]; then
  sudo mkdir -p /mnt/9front; sudo mount -o loop,ro 9front.iso /mnt/9front
  mkdir -p root; cp -a /mnt/9front/. root/; sudo umount /mnt/9front; chmod -R u+w root
fi
cp root/amd64/9pc64 9pc64
if [ ! -x u9fs ]; then
  [ -d plan9-src ] || git clone -q --depth=1 --filter=blob:none --sparse https://github.com/plan9foundation/plan9 plan9-src
  ( cd plan9-src && git sparse-checkout set sys/src/cmd/unix/u9fs && cd sys/src/cmd/unix/u9fs &&
    make CFLAGS="-g -I. -Wno-implicit-function-declaration -DBit5=2 -DRunemax=0x10FFFF" >/dev/null && cp u9fs "$N/u9fs" )
fi
# u9fs maps 9P users through the served tree's own /etc/passwd and /etc/group. run_9front.sh runs it in a user
# namespace (unshare -r) where only id 0 exists, standing for the invoking host user: list that user as 0.
mkdir -p root/etc
u=$(id -un); g=$(id -gn)
sed -i "/^$u:/d" root/etc/passwd 2>/dev/null || true; echo "$u:x:0:0::/:/bin/false" >> root/etc/passwd
sed -i "/^$g:/d" root/etc/group 2>/dev/null || true; echo "$g:x:0:" >> root/etc/group
echo "9front ready in $N (release $REL)"
