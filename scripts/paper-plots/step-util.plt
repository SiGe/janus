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
set ylabel "Cost Ratio"
set output "step-util-cost.pdf"
set yrange [0:1]
plot "<cat step-util.data | grep 0.711" using ($4/$6) title "70\%" ls 2 lw 1, \
     "<cat step-util.data | grep 0.814" using ($4/$6) title "80\%" ls 1 lw 1, \
     "<cat step-util.data | grep 0.908" using ($4/$6) title "90\%" ls 3 lw 1
