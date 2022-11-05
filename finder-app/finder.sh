#!/bin/sh
# Finder script for assignment 1
# Author: Allen Gonzalez

if [ $# -eq 2 ] 
then
	#echo "Correct number of arguments"
	FILESDIR=$1
	SEARCHSTR=$2
	cd ${FILESDIR}	
	X=$(ls | wc -l)
	Y=$(grep -r ${SEARCHSTR} * | wc -l)
	echo "The number of files are $X and the number of matching lines are $Y"

	exit 0

else
	echo "Need 2 arguments: filesdir and searchstr."
	exit 1
fi
