load "../common.plt"
load "../small.plt"

set style histogram clustered gap 2 #lw 1
set style data histogram
set style fill solid 0.6 border lt 0
set boxwidth 1

set datafile separator " "
set format x "%.0f%%"
set xlabel "Deadline (steps)"

set xtics ("2" 0, "4" 1, "8" 2)

set key outside top left horizontal
set ylabel "Steps Ratio"
set output "step-len.pdf"
set yrange [0:1]
plot "<cat step.data | grep 0.814 | grep -i janusdynamic" using ($4/$11) title "Janus Online/MRC" ls 2 lw 1, \
     "<cat step.data | grep 0.814 | grep -i janusstatic" using ($4/$11) title "Janus Offline/MRC" ls 1 lw 1

set key outside top left horizontal
set ylabel "Cost Ratio"
set output "step-cost.pdf"
set yrange [0:1]

plot "<cat step.data | grep 0.814 | grep -i janusdynamic" using ($6/$13) title "Janus Online/MRC" ls 2 lw 1, \
     "<cat step.data | grep 0.814 | grep -i janusstatic" using ($6/$13) title "Janus Offline/MRC" ls 1 lw 1

