#!/bin/bash

set -e

BASEDIR=$1
BUILDIR=$2
MYSQLDIR=$3
MYSQLTARGZ=$4

rm -rf $MYSQLDIR

cd $BUILDIR
tar xzvf $MYSQLTARGZ
cd $MYSQLDIR

#applying patches
echo "">$MYSQLDIR/sql/mysqld.cc
cp $BASEDIR/patches/mysqldlib.cc $BASEDIR/include/mysqldlib.h $MYSQLDIR/sql
cp $BASEDIR/patches/myisamreader.c $BASEDIR/include/myisamreader.h $MYSQLDIR/storage/myisam
patch -R -p0 < $BASEDIR/patches/makefileam.patch

autoreconf -ivf
./configure --prefix $MYSQLDIR/build --with-plugins=myisam,heap
make -j4

# installing 
mkdir -p $BUILDIR/lib
cp -a $MYSQLDIR/sql/.libs/libmysqldlib.{so,so.0,so.0.0.0} $BUILDIR/lib/
cp -a $MYSQLDIR/storage/myisam/.libs/libmyisamreader.{so,so.0,so.0.0.0} $BUILDIR/lib/


