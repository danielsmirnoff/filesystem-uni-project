#!/bin/bash
#A helper automation script to screenshot all blocks within a range
# !!! Must have gnome-screenshot installed
# !!! Images are saved to pictures directory
# !!! Press Ctrl + C to terminate

start_block=2
end_block=155


for((i=$start_block; i<=$end_block; i++)); do
    clear
    ./Hexdump/hexdumpl SampleVolume --start $i --count 1

    sleep 0.1

    gnome-screenshot -w

    sleep 0.1
done
