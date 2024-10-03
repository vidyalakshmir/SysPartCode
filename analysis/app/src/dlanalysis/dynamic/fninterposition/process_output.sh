#!/bin/bash

out=$1
newout=$2

touch temp.txt
touch temp1.txt
for FILE in $out/fninterp_*.txt
do
	line=$(grep 'mydlopen' $FILE | awk {'print $2'} | sed 's/(//' | sed 's/)//')
	for l in $line
	do
		echo $l >> temp.txt
	done
	sline=$(grep 'mydlsym' $FILE | awk {'print $2'} | sed 's/(//' | sed 's/)//')
	for s in $sline
	do
		echo $s >> temp1.txt
	done
	mv $FILE $newout
done
sort temp.txt | uniq > $newout/fninterp_dlopen.txt
rm temp.txt
sort temp1.txt | uniq > $newout/fninterp_dlsym.txt
rm temp1.txt
