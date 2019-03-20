# Runs the static experiments for LTG and PUG-long
#
# These show the spatial complexity of planning.  By default we use Azure's SLA
# function, but we can use others.

. ./common.bash

config() {
  file=$1
  cutoff=$2
  scale=$3

  set_kv ${file} risk-violation "${AZURE_COST}"
  set_kv ${file} criteria-time "cutoff-at-${cutoff}"
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo ${cutoff}
}

planners() {
  echo "ltg pug-long"
}

export -f planners
export -f config

parallel\
  executor experiments/01-paper-static-traffic.ini\
  config planners\
  ::: 4 8\
  ::: 0.9 1 1.1 1.2 1.3 |\
  column -t | sort -k1,1 -k2,2 -nk3,3 | tee data/01-static-experiment.log
