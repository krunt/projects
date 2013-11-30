package Algorithm;

use strict;
use warnings;

sub new {
    my ($class, $query, $pcontext, $join_field) = @_;
    return bless {
        pcontext => $pcontext,
        query    => $query,
        join_field => $join_field,
    }, $class;
}

1;
