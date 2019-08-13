load "../common.plt"
load "../small.plt"

set style histogram clustered gap 2 #lw 1
set style data histogram
set style fill solid 0.6 border lt 0
set boxwidth 1

set datafile separator " "
set format x "%.0f%%"
set xlabel "Cost Function"

set xtics  ("Staged-1" 0, "Staged-2" 1, "Staged-3" 2, "log" 3, "linear" 4, "quad." 5, "exp" 6) rotate by 30 center offset 1,-0.5

df='../data/09-failure-sweep.log'

# set key outside top left horizontal
# set ylabel "Steps Ratio"
# set output "failure-len.pdf"
# set yrange [0:1]
# plot "<cat failure.data | grep 0.908 | grep -i janusdynamic-8" using ($6/$15) title "Janus Online/MRC" ls 2 lw 1, \
#      "<cat failure.data | grep 0.908 | grep -i janusstatic-8" using ($6/$15) title "Janus Offline/MRC" ls 1 lw 1

set key outside top left horizontal
set ylabel "Cost Ratio"
set output "cost-cost.pdf"
set yrange [0:1.0]
set xtics font "Gill Sans, 7"


plot "<cat cost.data | grep 0.814 | grep -i janusdynamic-4" using ($8/$17) title "Janus/MRC" ls 2 lw 1

# plot "<cat cost.data | grep 0.814 | grep -i janusdynamic-4" using ($8/$17) title "Janus Online/MRC" ls 2 lw 1, \
#      "<cat cost.data | grep 0.814 | grep -i janusstatic-4" using ($8/$17) title "Janus Offline/MRC" ls 1 lw 1

#    "<cat failure.data | grep 0.908 | grep GOOGLE | grep -i janusdynamic-8" using ($8/$17) title "Janus Online/MRC" ls 2 lw 1, \
#    "<cat failure.data | grep 0.908 | grep GOOGLE | grep -i janusstatic-8" using ($8/$17) title "Janus Offline/MRC" ls 1 lw 1, \
#    "<cat failure.data | grep 0.908 | grep AZURE | grep -i janusdynamic-8" using ($8/$17) title "Janus Online/MRC" ls 2 lw 1, \
#    "<cat failure.data | grep 0.908 | grep AZURE | grep -i janusstatic-8" using ($8/$17) title "Janus Offline/MRC" ls 1 lw 1, \
#    "<cat failure.data | grep 0.908 | grep LINEAR | grep -i janusdynamic-8" using ($8/$17) title "Janus Online/MRC" ls 2 lw 1, \
#    "<cat failure.data | grep 0.908 | grep LINEAR | grep -i janusstatic-8" using ($8/$17) title "Janus Offline/MRC" ls 1 lw 1
#plot "<(cat ".df." | grep 0.908 | grep -i janusdynamic-8 | tr -s ' ')"  using 8:9 title "Janus Online"  ls 2 lw 1, \
#     "<(cat ".df." | grep 0.908 | grep -i janusstatic-8 | tr -s ' ')"   using 8:9 title "Janus Offline" ls 1 lw 1, \
#     "<(cat ".df." | grep 0.908 | grep -i ltg-8 | tr -s ' ')"           using 8:9 title "MRC"           ls 3 lw 1,


# set output "failure-cost.pdf"
# set ylabel "Cost"
# set yrange [0:55]
# set key inside bottom right
# plot "<(cat ".df." | grep 0.814 | grep -i janusdynamic-8 | tr -s ' ')"  using ($4*100):8 title "Janus Online" w lp ls 2, \
#      "<(cat ".df." | grep 0.814 | grep -i janusstatic-8 | tr -s ' ')"   using ($4*100):8 title "Janus Offline" w lp ls 1, \
#      "<(cat ".df." | grep 0.814 | grep -i ltg-8 | tr -s ' ')"           using ($4*100):8 title "MRC" w lp ls 3,\
#      "<(cat ".df." | grep 0.814 | grep -i janusdynamic-8 | tr -s ' ')"  using ($4*100):8:9 notitle w yerrorbars ls 2 lw 1, \
#      "<(cat ".df." | grep 0.814 | grep -i janusstatic-8 | tr -s ' ')"   using ($4*100):8:9 notitle w yerrorbars ls 1 lw 1, \
#      "<(cat ".df." | grep 0.814 | grep -i ltg-8 | tr -s ' ')"           using ($4*100):8:9 notitle w yerrorbars ls 3 lw 1

