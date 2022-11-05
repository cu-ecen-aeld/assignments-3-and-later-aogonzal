#!/bin/sh
# Finder script for assignment 1
# Author: Allen Gonzalez

if [ $# -eq 2 ] 
then
	#echo "Correct number of arguments"
	WRITEFILE=$1
	WRITESTR=$2
		
	DIR=${WRITEFILE%/*}
	mkdir ${DIR}
	
	touch ${WRITEFILE}
	echo ${WRITESTR} >> ${WRITEFILE}
		
	exit 0

else
	echo "Need 2 arguments: writefile and writestr."
	exit 1
fi
