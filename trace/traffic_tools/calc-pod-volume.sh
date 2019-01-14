#!/bin/bash

process_file() {
  FILE=$1
  INDEX=$2
  while read -r line
  do
    pod="$(echo $line | awk '{print $1}')"
    L="$(echo $line | awk '{print $2}')"
    H="$(echo $line | awk '{print $3}')"
    hosts="$(echo $line | awk '{print $4}')"

    cat ${FILE} | head -n $H | tail -n $L | awk "{sum += \$1} END {printf \"%s\t%s\t%d\t%d\n\", \"$pod\", \"$hosts\", sum, sum/$hosts}"
  done < $INDEX
}

usage() {
  echo <<EOF
  > $0 [traffic-file] [index-file]

  Output is a list of rows where each row contains:
  = PodName #ToRs TotalTraffic AvgTrafficPerTor
EOF
}


# - Generate tenants (by randomly sampling ToRs)
#    - Sample tenants from some distribution?
#    
# - Decide on an arrival / leave time for a tenant (????)
# - Build the TMs from the arrival and leave time of the tenants.


if [ $# != 2 ]; then
  usage
else
  process_file $1 $2
fi
