#!/bin/sh
if ./$1.exe >/tmp/$1.run 2>&1 
then
	mv /tmp/$1.run $1.run
	exit 0
else
	cat /tmp/$1.run
	rm /tmp/$1.run
	exit 1
fi
