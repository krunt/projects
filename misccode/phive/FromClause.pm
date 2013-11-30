package FromClause;

use strict;
use warnings;
use TabrefSubClause;
use SubQuerySubClause;
use JoinSubClause;
use Utils;

use constant {
    JOIN       => 1,
    TABREF     => 2,
    SUBQUERY   => 3,
};

sub new {
    my ($class, $type, $params) = @_;

    my $self;
    if ($type == TABREF()) {
        $self = TabrefSubClause->new($params);
    } elsif ($type == SUBQUERY()) {
        $self = SubQuerySubClause->new($params);
    } elsif ($type == JOIN()) {
        $self = JoinSubClause->new($params);
    }

    return bless {
        self => $self,
        type => $type,
    }, $class;
}

sub self {
    return shift->{self};
}

sub tablename {
    return shift->self()->tablename();
}

sub type {
    return shift->{type};
}

sub queries {
    return shift->{self}->queries();
}

sub query {
    return (shift->{self}->queries())[0];
}

1;
