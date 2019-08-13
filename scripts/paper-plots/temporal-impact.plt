load "../common.plt"
load "../small.plt"

set datafile separator " "
set format x "%.0f%%"
set xlabel "MLU"

df='../data/02-dynamic-experiment.log'

set key inside top left
set ylabel "Cost"
set yrange [0:55]
set output "temporal-impact-cost-4.pdf"
plot "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):6 title "Janus Offline" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using ($3*100):6 title "Janus" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):6 title "MRC" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using ($3*100):6:7 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using ($3*100):6:7 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using ($3*100):6:7 notitle w yerrorbars ls 3 lw 1


set output "temporal-impact-cost-8.pdf"
set ylabel "Cost"
set yrange [0:80]
set key inside top left
plot "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using ($3*100):6 title "Janus Offline" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')"  using ($3*100):6 title "Janus" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using ($3*100):6 title "MRC" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using ($3*100):6:7 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')"  using ($3*100):6:7 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using ($3*100):6:7 notitle w yerrorbars ls 3 lw 1
