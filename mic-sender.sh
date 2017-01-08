#!/bin/bash

arecord -v -t raw -f cd -B 100000 -D sysdefault | \
   ./runclient -s localhost "$@" > /dev/null
