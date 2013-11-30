package TabrefSubClause;

use strict;
use warnings;

use base qw(FromSubClause);

use Utils;

sub new {
    my ($class, $params) = @_;
    return bless {%$params}, $class;
}

sub tablename {
    my ($self) = @_;
    return Utils::strip($self->{tablename});
}

sub queries {
    return ();
}

1;
