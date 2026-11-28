# Build OpenTTD from its generated C, and install it (README.Plan9).
#	mk		build: FreeType (freetype), then the game (tu, with
#			the system's configuration), 6.openttd here,
#			next to its data
#	mk install	for yourself: $home/bin/$cputype/openttd, the data
#			in $home/lib/openttd
#	mk BIN=/$cputype/bin install
#			for everyone (as the host owner): the data in
#			/sys/lib/openttd
#	mk uninstall	the same, the other way
#	mk clean	the objects
# SYS is 9front where libc has atomic(2), else 9legacy; mk SYS=... to
# say which.

SYS=`{grep -s acasl /sys/include/libc.h && echo 9front || echo 9legacy}
BIN=$home/bin/$cputype
DATA=lang ai game media bin baseset

all:V:	6.openttd

6.openttd:V:
	@{cd freetype && mk}
	mkdir -p tu/obj.$SYS
	@{cd tu && mk 'SYS='$SYS}
	cp tu/obj.$SYS/6.openttd 6.openttd

install:V:	6.openttd
	lib=/sys/lib/openttd
	if(~ $BIN $home/bin/$cputype)
		lib=$home/lib/openttd
	mkdir -p $BIN
	cp 6.openttd $BIN/openttd
	for(d in $DATA){
		mkdir -p $lib/$d
		dircp $d $lib/$d
	}

uninstall:V:
	lib=/sys/lib/openttd
	if(~ $BIN $home/bin/$cputype)
		lib=$home/lib/openttd
	rm -f $BIN/openttd
	for(d in $DATA)
		rm -rf $lib/$d

clean:V:
	@{cd freetype && mk nuke}
	rm -rf tu/obj.* 6.openttd
