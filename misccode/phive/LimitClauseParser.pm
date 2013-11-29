package LimitClauseParser;

use strict;
use warnings;

use LimitClause ();

sub new {
    return bless {}, shift;
}

sub parse {
    my ($self, $raw_statement) = @_;
    return LimitClause->new($raw_statement);
}
1;

