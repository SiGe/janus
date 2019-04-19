#!/usr/bin/env bash

# Runs the dunamic experiments for LTG, PUG-long, PUG-Lookback
#
# This supposedly shows the complexity of the temporal aspect of the planning.

. ./common.bash

config() {
  file=$1
  bursty=$2
  scale=$3

  rest=`echo "${bursty}" | awk '{printf "%.1f", 1-$1}'`

  set_kv ${file} traffic-test "trace/data/bursty-8-12-0.3-400-${bursty}-${rest}-compressed/traffic"
  set_kv ${file} traffic-training "trace/data/bursty-8-12-0.3-400-${bursty}-${rest}-compressed/traffic"
  set_kv ${file} risk-violation "${AZURE_COST}"
  set_kv ${file} criteria-time "cutoff-at-4"
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo "${bursty}\t${scale}\t${file}"
}

planners() {
  echo "ltg pug-long pug-lookback"
}

export -f planners
export -f config

parallel --eta --progress\
  executor experiments/02-paper-dynamic-traffic.ini\
  config planners\
  ::: 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0\
  ::: 0.7 0.75 0.8 0.85 0.9 0.95 1 1.05 1.1 1.15 1.2 1.25 1.3 |\
  column -t | sort -k1,1 -nk2,2 -nk3,3 | tee data/14-bursty-more.log
