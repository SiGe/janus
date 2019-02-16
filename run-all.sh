#!/bin/bash

EXP=$1
STEPS=$2

set_mop_duration() {
  EXP=$1
  DURATION=$2
  sed -i "s/mop-duration =.*/mop-duration = ${DURATION}/g" experiments/${EXP}.ini
}

set_predictor() {
  EXP=$1
  PRED=$2
  sed -i "s/type =.*/type = ${PRED}/g" experiments/${EXP}.ini
}

set_steps() {
  EXP=$1
  TIME=$2
  sed -i "s/criteria-time=.*/criteria-time=cutoff-at-${TIME}/g" experiments/${EXP}.ini
}

run() {
  EXP=$1
  STEPS=$2

  set_mop_duration ${EXP} 1
  set_steps ${EXP} ${STEPS}

  echo "Running LTG."
  make && ./bin/netre experiments/${EXP}.ini -a ltg |& tee ltg.out >/dev/null

  echo "Running PUG perfect."
  set_predictor ${EXP} perfect
  make && ./bin/netre experiments/${EXP}.ini -a pug |& tee pug-perfect.out >/dev/null

  echo "Running PUG EWMA."
  set_predictor ${EXP} ewma
  make && ./bin/netre experiments/${EXP}.ini -a pug |& tee pug-ewma.out >/dev/null
}

stats_header() {
  printf "TYPE\t\t\tMIN\tMEAN\tMEDIAN\tMAX\n"
}

stats() {
  fname=$1
  result=$(cat $fname | grep "best plan" | cut -d':' -f 3 | awk '{print $1}' | datamash -R4 --sort min 1 mean 1 median 1 max 1)

  printf "$fname\t\t\t$result\n"
}

summarize() {
  stats_header
  stats pug-perfect.out
  stats pug-ewma.out
  stats ltg.out
}

main() {
  run $1 $2
  summarize | column -t
}

main $EXP $STEPS
