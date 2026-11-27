# Install OpenTTD (README.Plan9).
#	mk install	for yourself: $home/bin/$cputype/openttd, the data
#			in $home/lib/openttd
#	mk BIN=/$cputype/bin install
#			for everyone (as the host owner): the data in
#			/sys/lib/openttd
#	mk uninstall	the same, the other way
# The game finds its data in either place; its own files (configuration,
# saves) are always in $home/lib/openttd.

BIN=$home/bin/$cputype
DATA=lang ai game media bin baseset

install:V:
	if(~ $BIN $home/bin/$cputype)
		lib=$home/lib/openttd
	if not
		lib=/sys/lib/openttd
	mkdir -p $BIN
	cp 6.openttd $BIN/openttd
	for(d in $DATA){
		mkdir -p $lib/$d
		dircp $d $lib/$d
	}
	echo installed $BIN/openttd, data in $lib

uninstall:V:
	if(~ $BIN $home/bin/$cputype)
		lib=$home/lib/openttd
	if not
		lib=/sys/lib/openttd
	rm -f $BIN/openttd
	for(d in $DATA)
		rm -rf $lib/$d
	echo removed $BIN/openttd and the data in $lib
