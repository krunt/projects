package WhereClauseParser;

use strict;
use warnings;
use WhereClause;
use PConstants;
use Asserter;

sub new {
    return bless {}, shift;
}

sub parse {
    my ($self, $raw_statement, $pcontext) = @_;
    return WhereClause->new($raw_statement, $pcontext);
}

1;
