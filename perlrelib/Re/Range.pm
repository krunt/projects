package Re::Range;

use strict;
use warnings;

sub new {
    my ($class, $include_to_interval) = @_;
    return bless { min_range => [], 
                   max_range => [], 
           }, $class;
}

sub len {
    my ($self) = @_;
    return scalar @{$self->{min_range}};
}

sub insert {
    my ($self, $from, $to) = @_;
    $to ||= $from;
    ($from, $to) = sort {$a <=> $b} (ord($from), ord($to));
    my ($min, $max) = @$self{qw(min_range max_range)};
    for my $i (0 .. $self->len() - 1) {
        if ($from >= $min->[$i] && $to <= $max->[$i]) {
            return;
        } elsif ($from <= $min->[$i] && $to >= $max->[$i]) {
            $min->[$i] = $from;
            $max->[$i] = $to;
            return;
        } elsif ($from < $min->[$i] && $to >= $min->[$i]) {
            $min->[$i] = $from;
            return;
        } elsif ($from <= $max->[$i] && $to > $max->[$i]) {
            $max->[$i] = $to;
            return;
        }
    }
    #new one
    push(@$min, $from);
    push(@$max, $to);
}

sub min_range {
    my ($self, $i) = @_;
    return $self->{min_range}[$i];
}

sub max_range {
    my ($self, $i) = @_;
    return $self->{max_range}[$i];
}

1;

