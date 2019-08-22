#!/bin/bash

FILE=../data/09-failure-sweep.log
paste <(cat $FILE | grep -i janusdynamic-4 | tr -s ' ') <(cat $FILE | grep -i ltg-4 | tr -s ' ') >failure.data
paste <(cat $FILE | grep -i janusstatic-4 | tr -s ' ') <(cat $FILE | grep -i ltg-4 | tr -s ' ') >>failure.data
sed -i -e 's/\t/ /g' failure.data

staticAwkCommand='{
    val1 = $3;
    sum1 += val1;
    sumsq1 += (val1*val1); count+=1;

    val2 = $6;
    sum2 += val2;
    sumsq2 += (val2*val2);
    mlu=int(int($2*100)/5.0) * 5;
  } END {
    avg1 = sum1/count;
    std1 = sqrt(sumsq1/count - avg1*avg1);

    avg2 = sum2/count;
    std2 = sqrt(sumsq2/count - avg2*avg2);
    print mlu, avg1, std1, avg2, std2;
  }'

for grepExpr in '[[:space:]]0.7[0-4]' '[[:space:]]0.7[5-9]' '[[:space:]]0.8[0-4]' '[[:space:]]0.8[5-9]' '[[:space:]]0.9[0-4]' '[[:space:]]0.9[5-8]'; do
  cat ../data/15-static-many.log | grep "${grepExpr}" |\
    sort -nk2,2 -k1,1 |\
    awk '{print $1, $9, $12}' | paste - - |\
    awk "${staticAwkCommand}"
done | tee static-many.data


for grepExpr in '0\.656477\|0\.6536\|0\.6[6-7]' '0\.7[4-6][0-9][0-9][1-9]' '0\.8[1-2]' '0\.8[9][0,3]\|0.902'; do
  cat ../data/13-bursty.log | grep "$grepExpr" |\
    sort -nk4,4 | awk '{print $7, $1, $2, $4, $10}' | paste - - - |\
    awk '{printf "%.2f\t%f\t%f\t%f\n", int($1*50)/50.0, $4, $5, $15}'
done | tr -s $'\t' | tr -s " " | tee bursty.data

cat ../data/10-dynamic-experiment.log | grep -iv "static"  | awk '{print $2, $3, $4, $6}' | paste - - | awk '{print $1, $2, $3, $4, $7, $8}' >step-util.data

readScalabilityFile() {
  FILE=$1
  DataStr=$(cat ${FILE} | grep Statistics)
  LenAvg=$(echo "${DataStr}" | cut -d' ' -f4 | tr -d '(),')
  LenStd=$(echo "${DataStr}" | cut -d' ' -f5 | tr -d '(),')
  CstAvg=$(echo "${DataStr}" | cut -d' ' -f7 | tr -d '(),')
  CstStd=$(echo "${DataStr}" | cut -d' ' -f8 | tr -d '(),')

  printf "%s\t%s\t%s\t%s\t" "${LenAvg}" "${LenStd}" "${CstAvg}" "${CstStd}"
}

DIR=../data/scalability
for SCALE in 8-12 16-24 24-32 32-48; do
  printf "JanusDynamic\t${SCALE}\t"
  readScalabilityFile "${DIR}/scalability-${SCALE}-pug-lookback.log"
  readScalabilityFile "${DIR}/scalability-${SCALE}-ltg.log"
  printf "\n"

  printf "JanusStatic\t${SCALE}\t"
  readScalabilityFile "${DIR}/scalability-${SCALE}-pug-long.log"
  readScalabilityFile "${DIR}/scalability-${SCALE}-ltg.log"
  printf "\n"
done >scale.data

FILE=../data/09-failure-sweep.log
paste <(cat $FILE | grep -i janusdynamic-8 | tr -s ' ') <(cat $FILE | grep -i ltg-8 | tr -s ' ') >failure.data
paste <(cat $FILE | grep -i janusstatic-8 | tr -s ' ') <(cat $FILE | grep -i ltg-8 | tr -s ' ') >>failure.data
sed -i -e 's/\t/ /g' failure.data

FILE=../data/10-dynamic-experiment.log
paste <(cat $FILE | grep -i janusdynamic | tr -s ' ') <(cat $FILE | grep -i ltg | tr -s ' ') >step.data
paste <(cat $FILE | grep -i janusstatic | tr -s ' ') <(cat $FILE | grep -i ltg | tr -s ' ') >>step.data
sed -i -e 's/\t/ /g' step.data

FILE=../data/07-cost-cloud.log
paste <(cat $FILE | grep -i janusdynamic | tr -s ' ') <(cat $FILE | grep -i ltg | tr -s ' ') >cost.data
paste <(cat $FILE | grep -i janusstatic | tr -s ' ') <(cat $FILE | grep -i ltg | tr -s ' ') >>cost.data
sed -i -e 's/\t/ /g' cost.data

FILE=../data/04-cost-time.log
paste <(cat $FILE | grep -v nan | grep -i janusdynamic | tr -s ' ') <(cat $FILE | grep -v nan | grep -i ltg | tr -s ' ') >time.data
paste <(cat $FILE | grep -v nan | grep -i janusstatic | tr -s ' ') <(cat $FILE | grep -v nan | grep -i ltg | tr -s ' ') >>time.data
sed -i -e 's/\t/ /g' cost.data
