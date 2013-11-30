package JoinAlgorithm;

use strict;
use warnings;
use MRJoinReduce;

use base qw(Algorithm);

sub execute { 
    my $self = shift;

    return MRJoinReduce->new($self->{query}, $self->{pcontext}, 
              $self->{join_field})->execute();
}

1;
