#!/bin/bash

pypy main.py 8-12-0.3-400    8 12 0.3  400 ../facebook/webserver ../facebook/hadoop
pypy main.py 16-24-0.2-400  16 24 0.2  400 ../facebook/webserver ../facebook/hadoop
pypy main.py 24-32-0.15-400 24 32 0.15 400 ../facebook/webserver ../facebook/hadoop
pypy main.py 32-48-0.1-400  32 48 0.1  400 ../facebook/webserver ../facebook/hadoop

for i in $(ls -d */); do
    mkdir ${i%/}-compressed/;
    ../../bin/traffic_compressor $i ${i%/}-compressed/traffic;
done
