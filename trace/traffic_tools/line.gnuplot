set datafile separator '\t'

set key right bottom
set xlabel "Ticks"
set ylabel "Traffic vol"

set term pdfcairo
set output "traffic.pdf"
plot "stats.tsv" using 0:2 notitle with linespoints

set ylabel "User count"
set output "users.pdf"
plot "stats.tsv" using 0:1 notitle with linespoints

set ylabel "Pod 0 traffic"
set output "pod-0.pdf"
plot "stats.tsv" using 0:3 notitle with linespoints
