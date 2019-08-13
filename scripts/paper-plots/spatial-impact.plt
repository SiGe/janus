load "../common.plt"
load "../small.plt"

set datafile separator " "
set format x "%.0f%%"
set xlabel "MLU"

df='../data/01-static-experiment.log'

set key inside bottom left
set ylabel "Step count"
set output "spatial-impact-len.pdf"
plot "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):4 title "Janus Offline" w lp ls 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):4 title "MRC" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):4:5 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):4:5 notitle w yerrorbars ls 3 lw 1


set key inside bottom right
set output "spatial-impact-cost.pdf"
set ylabel "Cost"
set yrange [0:55]
plot "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):6 title "Janus Offline" w lp ls 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):6 title "MRC" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):6:7 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):6:7 notitle w yerrorbars ls 3 lw 1
