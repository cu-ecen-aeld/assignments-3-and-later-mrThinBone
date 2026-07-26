#!/bin/sh

if [ -z "$1" ]
  then
    echo "No target directory supplied"
    exit 1
fi

if [ -z "$2" ]
  then
    echo "No search string supplied"
    exit 1
fi

filedir=$1
searchstr=$2

if [ ! -d "$filedir" ]; then
    echo "Error: $filedir is not a directory"
    exit 1
fi

X=$(find "$filedir" -mindepth 1 | wc -l)

Y=$(grep -r "$searchstr" "$filedir" | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"

exit 0