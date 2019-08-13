load "common.plt"

set datafile separator " "
set xlabel x_label

set output filename."-len-8.pdf"
set ylabel "# Steps"
set key outside top left horizontal

plot "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using 3:4 title "Janus Static" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')" using 3:4 title "Janus Dynamic" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using 3:4 title "LTG" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using 3:4:5 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')" using 3:4:5 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using 3:4:5 notitle w yerrorbars ls 3 lw 1

set output filename."-cost-8.pdf"
set ylabel "Cost"
plot "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using 3:6 title "Janus Static" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')" using 3:6 title "Janus Dynamic" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using 3:6 title "LTG" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-8 | tr -s ' ')"  using 3:6:7 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-8 | tr -s ' ')" using 3:6:7 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-8 | tr -s ' ')"          using 3:6:7 notitle w yerrorbars ls 3 lw 1

set output filename."-cost-4.pdf"
plot "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using 3:6 title "Janus Static" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using 3:6 title "Janus Dynamic" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using 3:6 title "LTG" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using 3:6:7 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using 3:6:7 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using 3:6:7 notitle w yerrorbars ls 3 lw 1

set output filename."-len-4.pdf"
plot "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using 3:4 title "Janus Static" w lp ls 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using 3:4 title "Janus Dynamic" w lp ls 2, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using 3:4 title "LTG" w lp ls 3,\
     "<(cat ".df." | grep -i janusstatic-4 | tr -s ' ')"  using 3:4:5 notitle w yerrorbars ls 1 lw 1, \
     "<(cat ".df." | grep -i janusdynamic-4 | tr -s ' ')" using 3:4:5 notitle w yerrorbars ls 2 lw 1, \
     "<(cat ".df." | grep -i ltg-4 | tr -s ' ')"          using 3:4:5 notitle w yerrorbars ls 3 lw 1
