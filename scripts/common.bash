#!/usr/bin/env bash

function_exists() {
  declare -f -F $1 > /dev/null
  return $?
}

repeat_and_join() {
  REP=$1
  TIMES=$2
  SEP=$3

  res=""
  for i in `seq 2 ${TIMES}`; do
    res="${res}${REP}${SEP}"
  done

  echo "${res}${REP}"
}

tmp_file() {
  FILE=$1
  TEMP=$(mktemp)
  cat ${FILE} > ${TEMP}
  echo ${TEMP}
}

tmp_dir() {
  mktemp -d 2>/dev/null || mktemp -d -t 'mytmpdir'
}

get_bw() {
  FILE=$1
  string=$(get_kv $FILE network)
  echo ${string} | rev | cut -d'-' -f1 | rev
}

set_bw() {
  FILE=$1
  BW=$2

  string=$(get_kv $FILE network)
  topo=$(echo ${string} | rev | cut -d'-' -f2- | rev)
  set_kv $FILE network "${topo}-${BW}"
}

set_kv() {
  FILE=$1
  KEY=$2
  VALUE=$3
  sed -i "s@^${KEY}[[:space:]]*=.*@${KEY}=${VALUE}@g" ${FILE}
}

get_kv() {
  FILE=$1
  KEY=$2
  cat ${FILE} | grep "^${KEY}" | cut -d"=" -f2 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'
}

mult() {
  v1=$1
  v2=$2
  echo "${v1} * ${v2}" | bc -l
}

mult_int() {
  v1=$1
  v2=$2
  echo "scale=0; ${v1} * ${v2} / 1" | bc -l
}

netre() {
  ./bin/netre $@
}

cd_netre() {
  cd ../
}

get_mlu() {
  FILE=$1
  netre $FILE -a stats |& grep MLU | cut -d':' -f4
}

# Weird format for sure, but eh, let's just get it done.
# len: 4.000000 (0.000000), cost: 20.000000 (17.320508)
len() {
  echo "$1" | cut -d':' -f2 |awk '{print $1}' | tr -d '(),'
}

len_std() {
  echo "$1" | cut -d':' -f2 | awk '{print $2}' | tr -d '(),'
}

cost() {
  echo "$1" | cut -d':' -f3 | awk '{print $1}' | tr -d '(),'
}

cost_std() {
  echo "$1" | cut -d':' -f3 | awk '{print $2}' | tr -d '(),'
}

output_row() {
  file="$1"
  planner="$2"
  extra="$3"
  mlu="$4"

  echo -e "${names[${planner}]}-${extra}-steps\t${extra}\t${mlu}\t$(run_sim ${file} ${planner})"
}

run_sim() {
  FILE=$1
  SIM=$2
  line=$(netre ${FILE} -a ${SIM} |& grep "Statistics")
  echo "$(len "${line}")\t$(len_std "${line}")\t$(cost "${line}")\t$(cost_std "${line}")"
}

executor() {
  base_file=$1
  setter=$2
  get_planners=$3
  shift 3

  . ./common.bash
  . ./names.bash

  function_exists $setter || exit 1
  function_exists $get_planners || exit 1

  CWD=$(pwd)
  cd_netre

  file=$(tmp_file ${base_file})

  output=$($setter "${file}" $@)
  rv_dir="$(tmp_dir)/"
  set_kv ${file} rv-cache-dir "${rv_dir}"

  netre ${file} -a long-term >/dev/null 2>&1 
  mlu=$(get_mlu ${file})
  for planner in $($get_planners); do
    output_row "$file" "$planner" "$output" "$mlu"
  done

  rm -r "${rv_dir}"
}

export -f executor

# The power and ratios ensure that we get to 100 cost if the ToR loss gets to
# 100.  
LOGARITHMIC_COST='logarithmic-687.32-100-10-100'
EXPONENTIAL_COST='exponential-277.2588-100-10-100'
QUADRATIC_COST='poly-2-18000000-10-100'
LINEAR_COST="linear-50000-10-100"
AZURE_COST='stepped-0\/100-95\/25-99\/10-99.95\/0-100\/0'
AMAZON_COST='stepped-0\/30-99\/10-99.99\/0-100\/0'
GOOGLE_COST='stepped-0\/50-95\/25-99.0\/10-99.99\/0-100\/0'

NONE_TIME='8/0,0,0,0,0,0,0,0'
FIXED_TIME='8/4,4,4,4,4,4,4,4'
DEADLINE_TIME='8/0,0,0,0,0,0,30,30'
CRITICAL_TIME='8/1,2,3,4,5,6,7,8'
