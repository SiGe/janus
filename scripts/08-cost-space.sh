#!/usr/bin/env bash

# Runs the dunamic experiments for LTG, PUG-long, PUG-Lookback
#
# This supposedly shows the complexity of the temporal aspect of the planning.

. ./common.bash

config() {
  file=$1
  cutoff=$2
  scale=$3
  time_cost=$4
  cost=$5

  set_kv ${file} risk-violation "${!cost}"
  set_kv ${file} criteria-time "cutoff-at-${cutoff}/$(repeat_and_join ${time_cost} 8 ,)"
  set_kv ${file} concurrent-switch-failure 0
  set_kv ${file} concurrent-switch-probability 0.001
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo -e "${cutoff}\t${cost}\t${time_cost}"
}

planners() {
  #echo "pug-lookback"
  echo "pug-long ltg"
}

export -f planners
export -f config

 # ::: 0.8 1 1.2 1.4\
 # ::: AZURE_COST AMAZON_COST GOOGLE_COST LINEAR_COST  |\

parallel --eta --progress\
  executor experiments/02-paper-dynamic-traffic.ini\
  config planners\
  ::: 8\
  ::: 1\
  ::: 0 0.5 1 1.5 2 4 8 16\
  ::: AZURE_COST |\
  column -t | sort -k1,1 -nk2,2 -nk3,3 | tee data/08-cost-space.log
