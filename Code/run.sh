#!/bin/bash

EXEC=~/sniper/run-sniper
DIR=/tmp/workspace/sniper-$(date +%Y-%m-%d-%H-%M-%S)

SNIPER_ARGS="-c $1 -sstop-by-icount:1000000000"
# SNIPER_ARGS="-c tage -sstop-by-icount:1000000000"
# SNIPER_ARGS="-c nehalem-lite -sstop-by-icount:1000000"

mkdir $DIR
cd $DIR

for PINBALL in mcf astar sjeng bzip2 gobmk; do
    mkdir $PINBALL
    cd $PINBALL
    ~/sniper/run-sniper $SNIPER_ARGS --pinballs=/home/ubuntu/pinballs/$PINBALL/pinball.address >/dev/null 2>&1 &
    cd ..
done

while [ $(find -name "sim.out" | wc -l) -lt 5 ]; do
    echo $(find -name "sim.out" | wc -l)
    sleep 1
done
echo $(find -name "sim.out" | wc -l)

pkill pin
pkill nullapp

# wait

echo $DIR

# echo ============================================================
# cat astar/sim.cfg | grep -A 10 branch_predictor
# echo ============================================================

grep -HR "misprediction rate" | awk '{ printf "%s\t%6s\n", $1, $5}'
sum=$(grep -HR -A 4 "Branch predictor stats" | grep mpki | awk '{print $4}' | paste -sd+ | bc)
cnt=$(grep -HR "Branch predictor stats" | wc -l)
echo "Average MPKI: $(echo $sum/$cnt | bc -l)"
