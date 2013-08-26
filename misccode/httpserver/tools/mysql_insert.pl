#!/usr/bin/perl 

use strict;
use warnings;

open my $fd, "| mysql -u lua -ppass lua"
    or die $!;

while (<>) {
    chomp;
    my $line = $_;
    printf {$fd} "INSERT INTO lua_info VALUES ('%s', '%s', '%s');\n",
            "2012-07-10", "Morda", $line;
}
