#!/usr/bin/env bash

# Runs the dunamic experiments for LTG, PUG-long, PUG-Lookback
#
# This supposedly shows the complexity of the temporal aspect of the planning.

. ./common.bash

config() {
  file=$1
  cutoff=$2
  scale=$3

  set_kv ${file} risk-violation "${AZURE_COST}"
  set_kv ${file} criteria-time "cutoff-at-${cutoff}/4,4,4,4,4,4,4,4"
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo ${cutoff}
}

planners() {
  echo "ltg pug-long pug-lookback"
}

export -f planners
export -f config

parallel --eta --progress\
  executor experiments/02-paper-dynamic-traffic.ini\
  config planners\
  ::: 8\
  ::: 0.7 0.8 0.9 1 1.1 1.2 1.3 |\
  column -t | sort -k1,1 -nk2,2 -nk3,3 | tee data/04-plan-length-fix-function.log
