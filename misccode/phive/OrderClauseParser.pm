package OrderClauseParser;

use strict;
use warnings;

use OrderClause;

sub new { 
    return bless {}, shift; 
}

sub parse {
    my ($self, $raw_statement, $pcontext) = @_;

    my $direction = OrderClause::KW_DESC;
    my @statements;
    for my $child ($raw_statement->getChildren()) {
        if ($child->getName() != PConstants::TOK_COLREF()) {
            $direction = uc($child->getText()) eq 'ASC' 
                ? OrderClause::KW_ASC 
                : OrderClause::KW_DESC;
        }

        my ($table_name, $field_name) 
            = map { $_->getText() } $child->getChildren();

        unless ($field_name) {
            $field_name = $table_name;
            undef $table_name;
        }
        
        push(@statements, [$table_name, $field_name]); 
    }
     
    return OrderClause->new(\@statements, $direction, $pcontext);
}

1;

