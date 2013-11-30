package GroupClauseParser;

use strict;
use warnings;
use GroupClause;
use PConstants;
use Asserter;

sub new { 
    return bless {}, shift; 
}

sub parse {
    my ($self, $raw_statement, $pcontext) = @_;

    my @statements;
    for my $child ($raw_statement->getChildren()) {
        Asserter::assert_equals($child->getName(), PConstants::TOK_COLREF, 
            "Only table statements are allowed in Group by statement");

        my ($table_name, $field_name) 
            = map { $_->getText() } $child->getChildren();

        unless ($field_name) {
            $field_name = $table_name;
            undef $table_name;
        }
        
        push(@statements, [$table_name, $field_name]); 
    }
     
    return GroupClause->new(\@statements, $pcontext);
}

1;
