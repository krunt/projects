package LimitClause;

use strict;
use warnings;

sub new {
    my ($class, $rawlimit) = @_;
    return bless { limit => $rawlimit->getChild(0)->getText() }, $class;
}

sub limit {
    return $_[0]->{limit};
}

1;

