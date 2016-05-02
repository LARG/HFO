#!/bin/bash
set -e

if [ $# -lt 2 ]; then
    echo "usage: $0 log_file.rcg video_name.mp4"
    exit
fi

LOG=$1
OUT=$2
START_FRAME=0
SOCCERWINDOW="./bin/soccerwindow2"
DIR=`mktemp -d`

$SOCCERWINDOW -l $LOG --auto-image-save --canvas-size=1920x1200 --image-save-dir=$DIR
{
    ffmpeg -r 10 -start_number $START_FRAME -i $DIR/image-%05d.png -f mp4 -c:v libx264 -s 1024x768 -vf "crop=iw/2.5:8.38*ih/10:iw/2:ih/10,transpose=1" -pix_fmt yuv420p $OUT
} || {
    avconv -r 10 -i $DIR/image-%05d.png -f mp4 -c:v libx264 -s 1024x768 -vf "crop=iw/2.5:8.38*ih/10:iw/2:ih/10,transpose=1" -pix_fmt yuv420p $OUT
}

rm -fr $DIR
