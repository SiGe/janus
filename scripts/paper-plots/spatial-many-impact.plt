load "../common.plt"
load "../small.plt"

set datafile separator " "
set format x "%.0f%%"
set xlabel "MLU"

df='static-many.data'

set key inside top left
set output "spatial-impact-many-cost.pdf"
set ylabel "Cost"
set yrange [0:50]
plot "<(cat ".df.")"  using 1:2 title "Janus" w lp ls 1, \
     "<(cat ".df.")"  using 1:4 title "MRC" w lp ls 3,\
     "<(cat ".df.")"  using 1:2:3 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df.")"  using 1:4:5 notitle w yerrorbars ls 3 lw 1
