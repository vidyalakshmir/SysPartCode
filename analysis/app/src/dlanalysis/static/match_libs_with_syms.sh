#!/bin/bash
f=$1

script_dir=$(dirname "$0")

myfile="$script_dir/../unique_libs.txt"

tail -n +5 $f | while read -r line; do
	func=$(echo "$line" | awk '{print $1}')
	addr=$(echo "$line" | awk '{print $2}')
	sym=$(echo "$line" | awk '{print $3}')
	
	if [ "$sym" != "UNKNOWN" ] && [ "$sym" != "NULL" ]; then 
		while IFS= read -r line1
			do
				b=( $line1 )
				ln=$(readelf -s "$b" 2> /dev/null | grep -w "$sym" | grep -v 'UND' | wc -l)
				#echo "line $b $ln"
				if [ $ln -gt 0 ]
				then
					echo "$func $addr $sym $b"
				fi
			done < "$myfile"
	fi
done