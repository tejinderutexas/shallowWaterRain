#!/bin/sh
if [ ! -d "build" ]; then
  ./buildit.sh
fi
build/bin/assignment $1 $2
