#!/bin/sh
rm -f $1.pass $1.fail
./$1.exe >/tmp/$1.run 2>&1 
if test $? -eq 0
then
	mv /tmp/$1.run $1.pass
	exit 0
else
	cat /tmp/$1.run
	mv /tmp/$1.run $1.fail
	exit 1
fi
