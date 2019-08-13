load "../common.plt"
load "../small.plt"

set style histogram clustered gap 2 #lw 1
set style data histogram
set style fill solid 0.6 border lt 0
set boxwidth 1

set datafile separator "	"
set format x "%.0f"
set xlabel "Portion of Webserver traffic"

set key outside top left horizontal
set ylabel "Cost Ratio"
set output "bursty-cost.pdf"
set xtics font "Gill Sans, 7"

# plot "<cat bursty.data | grep '0\.64[[:space:]]\\|0\\.66[[:space:]]'" using 3:xtic(2) title "66\%" ls 2 lw 1, \
#      "<cat bursty.data | grep '0\.74[[:space:]]'"          using 3:xtic(2) title "74\%" ls 1 lw 1, \
#      "<cat bursty.data | grep '0\.82[[:space:]]\\|0\\.80[[:space:]]'" using 3:xtic(2) title "82\%" ls 3 lw 1, \
#      "<cat bursty.data | grep '0\.90[[:space:]]\\|0\\.88[[:space:]]'" using 3:xtic(2) title "90\%" ls 4 lw 1

set xtics ("0%%" 0, "20%%" 1, "40%%" 2, "60%%" 3, "80%%" 4, "100%%" 5)
set yrange [0:]

set yrange [0:1]
set ytics 0,0.2,1
set output "bursty-ratio-66.pdf"
plot "<cat bursty-new.data | grep '0\.64[[:space:]]\\|0\\.66[[:space:]]'" using ($3/$4) title "Janus/MRC" ls 2 lw 1

set output "bursty-ratio-74.pdf"
plot "<cat bursty-new.data | grep '0\.74[[:space:]]'" using ($3/$4) title "Janus/MRC" ls 2 lw 1

set output "bursty-ratio-82.pdf"
plot "<cat bursty-new.data | grep '0\.82[[:space:]]\\|0\.80[[:space:]]'" using ($3/$4) title "Janus/MRC" ls 2 lw 1

set output "bursty-ratio-90.pdf"
plot "<cat bursty-new.data | grep '0\.90[[:space:]]\\|0\.88[[:space:]]'" using ($3/$4) title "Janus/MRC" ls 2 lw 1
