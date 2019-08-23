load "../common.plt"
load "../small.plt"

set style histogram clustered gap 2 #lw 1
set style data histogram
set style fill solid 0.6 border lt 0
set boxwidth 1

set datafile separator "\t"
set format x "%.0f%%"
set xlabel "Time Cost Function"

set xtics ("Default" 3, "Const." 2, "Increasing" 0, "Deadline" 1) rotate by 25 center offset 0,-0.5;

set key outside top left horizontal
set ylabel "Cost Ratio" offset 1.5,0;
set output "time-cost.pdf"
set yrange [0:1.0]
set xtics font "Gill Sans, 7"

plot "<cat time.data | grep 0.814 | grep -i janusdynamic" using ($8/$17) title "Janus Online/MRC" ls 2 lw 1, \
     "<cat time.data | grep 0.814 | grep -i janusstatic" using  ($8/$17) title "Janus Offline/MRC" ls 1 lw 1
