package AlgorithmFactory;

use strict;
use warnings;

use SelectAlgorithm;
use JoinAlgorithm;
use GroupAlgorithm;
use GroupJoinAlgorithm;

sub new {
    return bless {}, shift;
}

sub create {
    my ($self, $query, $pcontext, $join_field) = @_;
    my $is_join    = ($query->from()->type() == FromClause::JOIN);
    my $is_groupby = $query->groupby();

    if (!$is_join && !$is_groupby) {
        return SelectAlgorithm->new($query, $pcontext, $join_field);
    }

    if ($is_join && !$is_groupby) {
        return JoinAlgorithm->new($query, $pcontext, $join_field);
    }

    if (!$is_join && $is_groupby) {
        return GroupAlgorithm->new($query, $pcontext, $join_field);
    }

    return GroupJoinAlgorithm->new($query, $pcontext, $join_field);
}

1;
