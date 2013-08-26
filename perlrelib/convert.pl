#!/usr/bin/perl 
use strict;
use warnings;

my $rule = shift || 1;

while (<>) {
    chomp;
    my ($pat, $str, $yn, $var, $res, $rest) = split /\t/, $_;
    if ($rule == 1) {
        # generate matching
        if ($var eq '-') {
            printf("[ '%s', '%s', 0 ],\n", $str, $pat);
        }
    } elsif ($rule == 2) {
        # generate matching with grouping
        if ($yn eq 'y' && $var eq '$&') {
            printf("[ '%s', '%s', 0, '%s' ],\n", $str, $pat, $res);
        }
    }
}

