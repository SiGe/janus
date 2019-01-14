#!/bin/bash

TRAFFIC="000000412.tsv"

while read -r line
do
  pod="$(echo $line | awk '{print $1}')"
  L="$(echo $line | awk '{print $2}')"
  H="$(echo $line | awk '{print $3}')"
  hosts="$(echo $line | awk '{print $4}')"

  cat ${TRAFFIC} | head -n $H | tail -n $L | awk "{sum += \$1} END {printf \"%s\t%s\t%d\t%d\n\", \"$pod\", \"$hosts\", sum, sum/$hosts}"
done < pod-index.tsv
