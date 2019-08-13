#!/bin/bash

echo -e "> Downloading traffic data."
wget -rc -nd "https://omid.io/graphs/janus/data.tar.gz"

echo -e "> Uncompressing the traffic data."
mkdir -p trace/
mv data.tar.gz trace/
cd trace/
tar -xzf data.tar.gz

echo -e "> Done."
