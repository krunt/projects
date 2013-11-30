package Asserter;

use strict;
use warnings;
use Carp qw(confess);

sub assert {
    my ($lhs, $message) = @_;
    $message ||= "Assertion failed";

    Carp::confess($message)
        unless ($lhs);
}

sub assert_equals {
    my ($lhs, $rhs, $message) = @_;
    $message ||= "Assertion failed";

    Carp::confess($message)
        unless ($lhs == $rhs);
}

1;
