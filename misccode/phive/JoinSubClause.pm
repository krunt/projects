package JoinSubClause;

use strict;
use warnings;

use base qw(FromSubClause);

sub new {
    my ($self, $params) = @_;
    return bless {
        %$params,
    }, $self;
}

sub queries {
    my $self = shift;
    return ($self->{queryA}, $self->{queryB});
}

sub get_join_field {
    my ($self, $query_name) = @_;
    for (0..1) {
        $self->{join_expression}[$_][0] eq $query_name
            and return $self->{join_expression}[$_][1];
    }

    return;
}

sub join_type {
    return shift->{join_type};
}

1;
