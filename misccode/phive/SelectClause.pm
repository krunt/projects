package SelectClause;

use strict;
use warnings;

use MapSubClause; 
use ReduceSubClause; 
use SelectSubClause;

use constant {
    MAP       => 1,
    REDUCE    => 2,
    SELECT    => 3,
};

sub new {
    my ($class, $type, $params, $pcontext, $distinct) = @_;

    my $self;
    if ($type == MAP()) {
        $self = MapSubClause->new($params, $pcontext);
    } elsif ($type == REDUCE()) {
        $self = ReduceSubClause->new($params, $pcontext);
    } elsif ($type == SELECT()) {
        $self = SelectSubClause->new($params, $pcontext);
    }

    return bless {
        self     => $self,
        type     => $type,
    }, $class;
}

sub is_map {
    return shift()->{type} eq MAP();
}

sub is_reduce {
    return shift()->{type} eq REDUCE();
}

sub field_names {
    return shift()->{self}->field_names();
}

sub field_codes {
    return shift()->{self}->field_codes();
}

sub get_item_by_alias {
    my ($self, $alias) = @_; 
    for my $item (@{$self->{self}{items} || []}) {
        if ($item->field_name() eq $alias) {
            return $item;
        }
    }
    return;
}

1;
