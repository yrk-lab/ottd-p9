#!/bin/bash
# setup_9front
#	One-time setup for run_9front.sh: the 9front amd64 root tree,
#	kernel and u9fs, in $nine (default ~/9front).  Needs sudo for
#	apt and for loop-mounting the ISO.  rel picks the release.
set -euo pipefail
. "$(dirname "$0")/../common.sh"

rel=${rel:-11952}
mkdir -p "$nine"
cd "$nine"

sudo apt-get install -y --no-install-recommends qemu-system-x86 gcc git curl >/dev/null

if [ ! -f 9front.iso ]; then
	curl -fL -o 9front.iso.gz "https://9front.org/iso/9front-$rel.amd64.iso.gz"
	gunzip -f 9front.iso.gz
fi

if [ ! -d root/rc ]; then
	sudo mkdir -p /mnt/9front
	sudo mount -o loop,ro 9front.iso /mnt/9front
	mkdir -p root
	cp -a /mnt/9front/. root/
	sudo umount /mnt/9front
	chmod -R u+w root
fi
cp root/amd64/9pc64 9pc64

if [ ! -x u9fs ]; then
	[ -d plan9-src ] ||
		git clone -q --depth=1 --filter=blob:none --sparse https://github.com/plan9foundation/plan9 plan9-src
	git -C plan9-src sparse-checkout set sys/src/cmd/unix/u9fs
	make -C plan9-src/sys/src/cmd/unix/u9fs \
		CFLAGS="-g -I. -Wno-implicit-function-declaration -DBit5=2 -DRunemax=0x10FFFF" >/dev/null
	cp plan9-src/sys/src/cmd/unix/u9fs/u9fs u9fs
fi

# u9fs looks users up in the served tree's own /etc/passwd and
# /etc/group.  run_9front.sh runs it in a user namespace where id 0,
# the only id, stands for the user running it: list that user as 0.
mkdir -p root/etc
u=$(id -un)
g=$(id -gn)
touch root/etc/passwd root/etc/group
sed -i "/^$u:/d" root/etc/passwd
sed -i "/^$g:/d" root/etc/group
echo "$u:x:0:0::/:/bin/false" >>root/etc/passwd
echo "$g:x:0:" >>root/etc/group

echo "9front $rel ready in $nine"
