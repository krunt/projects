package Re::Program;

use strict;
use warnings;

sub new {
    my ($class, $compiled_re, $parens) = @_;
    return bless {
        instruction => $compiled_re, 
        parens => $parens ? $parens : -1,
    }, $class;
}

sub instruction {
    my ($self) = @_;
    return $self->{instruction};
}

sub parens {
    my ($self) = @_;
    return $self->{parens};
}

1;

