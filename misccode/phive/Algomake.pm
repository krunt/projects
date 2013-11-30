package Algomake;

use strict;
use warnings;

sub new {
    my ($class) = @_;
    return bless {
        commands => [],
    }, $class;
}

sub add_command {
    my ($self, $command) = @_;
    push(@{$self->{commands}}, $command);
}

sub make {
    my ($self, $is_local_mode) = @_;
    if ($is_local_mode) {
        $self->make_for_local();
    } else {
        $self->make_for_mpppoc();
    }
}

sub make_for_local {
    my ($self) = @_;

    my $algorithm = <<EOF;
#!/bin/bash
set -x

EOF

    for my $command (@{$self->{commands}}) {
        $algorithm .= $command->string_for_local();
    }

    return $algorithm;
}

sub make_for_mpppoc {
    my ($self) = @_;

    my $algorithm = <<EOF;
#!/usr/bin/mpppoc 

EOF

    for my $command (@{$self->{commands}}) {
        $algorithm .= $command->string_for_mpppoc();
    }

    return $algorithm;
}

1;
