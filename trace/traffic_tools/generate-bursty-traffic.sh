#!/bin/bash

#for R in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0; do
for R in 0.1 0.3 0.5 0.7 0.9; do
  P=`echo "${R}" | awk '{printf "%.1f", 1-$1}'`
  DIR=bursty-8-12-0.3-400-${R}-${P}
  pypy main.py ${DIR} 8 12 0.3  400 ../facebook/webserver ../facebook/hadoop ${R} ${P}
  mkdir ${DIR}-compressed/
  ../../bin/traffic_compressor ${DIR}/ ${DIR}-compressed/traffic
done
