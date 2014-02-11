#!/bin/bash

set -e

$HADOOP_HOME/bin/hadoop fs -rmr jobresults d1 d2 d3 d4 d5 d6 d7 d8 d9 d0 d10 d11 d12 d13 d14 d15 || true
rm -rf $HADOOP_HOME/mine/jobconfdir/*

#-Dmapred.cache.files=file://localhost/home/akuts/hadoop12/dicts/UrlToGroups.yaml

HADOOP_CONF_DIR=$HOME/hadoop12/mine/conf \
PIG_CLASSPATH=$HOME/hadoop12/myclasses.jar \
PIG_OPTS=" -Djava.library.path=/usr/local/share/java/lib-dynload -Dunder.hadoop=1 " \
$PIG_HOME/bin/pig -optimizer_off ColumnMapKeyPrune -x mapreduce "$@" 2>&1 | tee log
#../bin/pig -optimizer_off ColumnMapKeyPrune -secretDebugCmd -libjars tutorial1.jar -x mapreduce "$@" 2>&1 | tee log

if grep "Global sort: true" log ; then 
    echo -e "Global sort is not supported.\nDo you really need it?"
    exit 1
fi

echo "moving jobconfdir contents"
jobident=$(cat /dev/urandom | head -c 128 | md5sum | cut -f1,1 -d ' ')

jobdir=$PIG_HOME/mine/jobhistory/$jobident
mkdir -p $jobdir

cp -r $HADOOP_HOME/mine/jobconfdir/* $jobdir

sed -i 's/[^[:print:]]//g' $jobdir/joblist

echo "generating shell for $jobident"
$PIG_HOME/mine/generate_shell.pl "$jobident" "$@"
