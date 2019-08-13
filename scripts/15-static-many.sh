# Runs the static experiments for LTG and PUG-long
#
# These show the spatial complexity of planning.  By default we use Azure's SLA
# function, but we can use others.

. ./common.bash

config() {
  file=$1
  time=$2
  cutoff=$3
  scale=$4

  set_kv ${file} traffic-test "trace/data/8-12-0.3-400-${time}-static-compressed/traffic"
  set_kv ${file} traffic-training "trace/data/8-12-0.3-400-${time}-static-compressed/traffic"
  set_kv ${file} risk-violation "${AZURE_COST}"
  set_kv ${file} criteria-time "cutoff-at-${cutoff}"
  set_bw ${file} $(mult_int $(get_bw $file) $scale)

  echo "${scale}\t${time}\t${file}\t${cutoff}"
}

planners() {
  echo "ltg pug-long"
}

export -f planners
export -f config

parallel --eta --progress\
  executor experiments/01-paper-static-traffic.ini\
  config planners\
  ::: `seq -s ' ' 50 5 295`\
  ::: 4\
  ::: 0.8 0.9 1 1.1 1.2 1.3 1.4 1.5 |\
  column -t | sort -k1,1 -k2,2 -nk3,3 | tee data/15-static-many.log
