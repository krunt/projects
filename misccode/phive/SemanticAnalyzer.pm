package SemanticAnalyzer;

use strict;
use warnings;
use QueryClauseParser;

sub new {
    my ($class) = @_;
    return bless {}, $class;
}

sub parse {
    my ($self, $tree, $pcontext) = @_;

    return QueryClauseParser->new()->parse(
                $tree, PConstants::MAIN_SELECT, $pcontext);
}

1;
