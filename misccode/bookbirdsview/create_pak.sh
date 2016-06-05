#!/bin/bash

set -e
set -x

pdffile="$1"
dstdir="$2"

if [ -z "$pdffile" ] || [ -z "$dstdir" ] ; then
    echo "no pdf file or dir are specified!"
    exit 1
fi

dstdir=${dstdir%%/*}

export MAGICK_TMPDIR=/mnt/usb/tempdir/

convert $pdffile $dstdir/im.tga

for i in $dstdir/*.tga ; do
    convert -type Grayscale -resize  595x842\! $i $i
done

convert -type Grayscale -resize  595x842\! "scratch.tga" "$dstdir/im--1.tga"

./make_pak $dstdir


