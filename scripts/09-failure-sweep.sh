#!/usr/bin/env bash

# Runs the dunamic experiments for LTG, PUG-long, PUG-Lookback
#
# This supposedly shows the complexity of the temporal aspect of the planning.

. ./common.bash

config() {
  file=$1
  cutoff=$2
  error=$3
  scale=$4

  set_kv ${file} risk-violation "${AZURE_COST}"
  set_kv ${file} criteria-time "cutoff-at-${cutoff}"
  set_kv ${file} concurrent-switch-failure 6
  set_kv ${file} concurrent-switch-probability "${error}"
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo -e "${cutoff}\t${error}"
}

planners() {
  echo "ltg pug-long pug-lookback"
}

export -f planners
export -f config

parallel --eta --progress\
  executor experiments/02-paper-dynamic-traffic.ini\
  config planners\
  ::: 4\
  ::: 0.01 0.02 0.03 0.04 0.05\
  ::: 1 |\
  column -t | sort -k1,1 -nk2,2 -nk3,3 | tee data/09-failure-sweep.log
