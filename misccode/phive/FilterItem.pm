package FilterItem;

use strict;
use warnings;
use Utils;

sub new {
    my ($class, $raw_statement) = @_;
    return bless {
        name => $raw_statement->[0],
        params  => [map {Utils::strip($_)} @{$raw_statement}[1..$#$raw_statement]],
    }, $class;
}

sub name {
    return shift->{name};
}

sub params {
    return @{shift->{params}};
}

1;
