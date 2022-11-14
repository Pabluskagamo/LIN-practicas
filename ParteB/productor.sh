#!/bin/bash
i=0
while true 
do
	for ((i=0; $i<=100; i++))
	do	
		echo $i > /dev/prodcons
		sleep 4
	done
done
