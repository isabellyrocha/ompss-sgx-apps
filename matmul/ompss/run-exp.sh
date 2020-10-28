#!/bin/bash

PROGRAM=matmul-p
default_size=32
default_block=8
default_threads=1
default_scheduler="bf"

#sizes=[1024, 512, 16]
#blocks=[1024, 512 , 16]
#threads=[1, 2, 4, 8]
#schedulers=["bf", "wf"]

#make

for iteration in {1..10};
do
  for size in 32 16 8
  do
    echo "ompss,size,$iteration,$size,$default_block,$default_threads,$default_scheduler,"
    NX_SMP_WORKERS=$default_threads NX_ARGS="--schedule=$default_scheduler" ./$PROGRAM $size $default_block
    sleep 1s
  done

  for block in 32 16 8
  do
    echo "ompss,block,$iteration,$default_size,$block,$default_threads,$default_scheduler,"
    NX_SMP_WORKERS=$default_threads NX_ARGS="--schedule=$default_scheduler" ./$PROGRAM $default_size $block
    sleep 1s
  done

  for threads in 1 2 4 8
  do
    echo "ompss,threads,$iteration,$default_size,$default_block,$threads,$default_scheduler,"
    NX_SMP_WORKERS=$threads NX_ARGS="--schedule=$default_scheduler" ./$PROGRAM $default_size $default_block
    sleep 1s
  done

  for scheduler in "bf" "wf"
  do
    echo "ompss,scheduler,$iteration,$default_size,$default_block,$default_threads,$scheduler,"
    NX_SMP_WORKERS=$default_threads NX_ARGS="--schedule=$scheduler" ./$PROGRAM $default_size $default_block
    sleep 1s
  done
done
make clean
