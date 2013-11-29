package SelectAlgorithm;

use strict;
use warnings;

use base qw(Algorithm);
use MRSimpleMapSelect;
use MRSimpleReduceSelect;
use MRSimpleSelect;

sub execute { 
    my $self = shift;

    if ($self->{query}->select()->is_map()) {
        return MRSimpleMapSelect->new($self->{query}, $self->{pcontext}, 
                $self->{join_field})->execute();
    } elsif ($self->{query}->select()->is_reduce()) {
        return MRSimpleReduceSelect->new($self->{query}, 
                $self->{pcontext}, $self->{join_field})->execute();
    } else {
        return MRSimpleSelect->new($self->{query}, $self->{pcontext}, 
                    $self->{join_field})->execute();
    }
}

1;
