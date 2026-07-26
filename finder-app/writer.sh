#!/bin/sh

if [ -z "$1" ]
  then
    echo "No target path supplied"
    exit 1
fi

if [ -z "$2" ]
  then
    echo "No write content supplied"
    exit 1
fi

writefile=$1
writestr=$2

rm -rf "$writefile"

directories=$(dirname "$writefile")

# 2. Create the missing directory safely
mkdir -p "$directories"

# 3. Create the file
touch "$writefile"

echo "$writestr" > "$writefile"

exit 0
