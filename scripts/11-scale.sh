#!/usr/bin/env bash

. ./common.bash

CWD=$(pwd)
cd_netre

for SCALE in 8-12; do # 16-24 24-32 32-48; do
  ./bin/netre experiments/${SCALE}.ini -a ltg |& tee scripts/data/scalability/scalability-${SCALE}-ltg.log
  ./bin/netre experiments/${SCALE}.ini -a pug-long |& tee scripts/data/scalability/scalability-${SCALE}-pug-long.log
  ./bin/netre experiments/${SCALE}.ini -a pug-lookback |& tee scripts/data/scalability/scalability-${SCALE}-pug-lookback.log
done
cd ${CWD}
