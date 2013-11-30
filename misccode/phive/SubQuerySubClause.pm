package SubQuerySubClause;

use strict;
use warnings;

use base qw(FromSubClause);

sub new {
    my ($self, $params) = @_;
    return bless {
        query => $params->{query},
    }, $self;
}

sub tablename {
    my $self = shift;
    return $self->{query}->insert()->tablename();
}

sub queries {
    my $self = shift;
    return ($self->{query});
}

1;
