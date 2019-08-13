# Line style for axes
set style line 80 lt rgb "#808080"

# Line style for grid
set style line 81 lt 0  # dashed
set style line 81 lt rgb "#808080"  # grey

set grid back linestyle 81
set border 3 back linestyle 80 # Remove border on top and right.  These
                               # borders are useless and make it harder
                               # to see plotted lines near the border.
                               # Also, put it in grey; no need for so much
                               # emphasis on a border.
set xtics nomirror
set ytics nomirror

#set log x
#set mxtics 10    # Makes logscale look good.


set key bottom right

# set xrange [0:1]
# set yrange [0:1]
#
