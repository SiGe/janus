. ./common.bash

f=$(tmp_file ../experiments/8-12.ini)
set_kv $f criteria-time cutoff-at-12
set_kv $f risk-delay dip-at-omid

func="set_kv"

${func} ${f} criteria-time woah!
get_kv ${f} criteria-time

mult_int 12.31 231.31132234
cat $f
