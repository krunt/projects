package FromClauseParser;

use strict;
use warnings;
use FromClause;
use QueryClauseParser;
use PConstants;
use Asserter;

sub new { return bless {}, shift; }

sub parse {
    my ($self, $raw_statement, $pcontext) = @_;

    my $current_alias = $pcontext->current_alias();
    my $from_clause;
    if ($raw_statement->getName() == PConstants::TOK_TABREF) {
        #it is table name

        my $tablealias = $raw_statement->getChild(1)->getText();
        $from_clause = FromClause->new(
                    FromClause::TABREF(), 
                    {
                        tablename  => $raw_statement->getChild(0)->getText(),
                        tablealias => $tablealias,
                    },
                );
        #setting from table here to calm down parser
        #$pcontext->db()->add_table(Table->new($tablealias));

        $pcontext->db()->get($current_alias)->set_from_tables($tablealias);

    } elsif ($raw_statement->getName() == PConstants::TOK_SUBQUERY) {
        my ($query_raw_statement, $alias) 
            = get_them_from_subquery($raw_statement);
        $from_clause = FromClause->new(
                        FromClause::SUBQUERY(), 
                        {
                            query => QueryClauseParser->new()->parse(
                                        $query_raw_statement, $alias, $pcontext),
                        });
        $pcontext->db()->get($current_alias)->set_from_tables($alias);

    } elsif (   $raw_statement->getName() == PConstants::TOK_JOIN
             || $raw_statement->getName() == PConstants::TOK_LEFTOUTERJOIN
             || $raw_statement->getName() == PConstants::TOK_RIGHTOUTERJOIN
             || $raw_statement->getName() == PConstants::TOK_FULLOUTERJOIN
            ) {
        Asserter::assert_equals(scalar $raw_statement->getChildren(), 3,
                            "invalid join statement");
        Asserter::assert_equals($raw_statement->getChild(0)->getName(),
                PConstants::TOK_SUBQUERY, "invalid join statement");
        Asserter::assert_equals($raw_statement->getChild(1)->getName(),
                PConstants::TOK_SUBQUERY, "invalid join statement");
        Asserter::assert($raw_statement->getChild(2)->getText() eq '=', 
                    "invalid join statement");

        my ($query_raw_statementA, $aliasA) 
            = get_them_from_subquery($raw_statement->getChild(0));
        my ($query_raw_statementB, $aliasB) 
            = get_them_from_subquery($raw_statement->getChild(1));
        my ($join_expression)
            = get_join_expression($raw_statement->getChild(2));

        $from_clause = FromClause->new(
                        FromClause::JOIN(), 
                        {
                            join_type => $raw_statement->getName(),
                            join_expression => $join_expression,
                            queryA => QueryClauseParser->new()->parse(
                                        $query_raw_statementA, $aliasA, $pcontext),
                            queryB => QueryClauseParser->new()->parse(
                                        $query_raw_statementB, $aliasB, $pcontext),
                        });

        $pcontext->db()->get($current_alias)->set_from_tables($aliasA, $aliasB);
    } 

    return $from_clause if $from_clause;

    Asserter::assert(0, "Unknown from statement")
}

sub get_them_from_subquery {
    my ($raw_statement) = @_;

    Asserter::assert_equals($raw_statement->getChild(0)->getName(), 
            PConstants::TOK_QUERY, "it is not query clause in subquery");

    my ($query_raw_statement, $alias) = $raw_statement->getChildren();
    return ($query_raw_statement, $alias->getText());
}

sub get_join_expression {
    my ($raw_statement) = @_;

    Asserter::assert_equals($raw_statement->size(), 2,
            "invalid join expression");
    Asserter::assert_equals($raw_statement->getChild(0)->getName(),
                PConstants::TOK_COLREF, "invalid join expression");
    Asserter::assert_equals($raw_statement->getChild(1)->getName(),
                PConstants::TOK_COLREF, "invalid join expression");
    Asserter::assert_equals($raw_statement->getChild(0)->size(),
                2, "invalid join COL_REF expression");
    Asserter::assert_equals($raw_statement->getChild(1)->size(),
                2, "invalid join COL_REF expression");

    my @expressions;
    for my $i (0..1) {
        push(@expressions, [ map {$_->getText()} 
                                $raw_statement->getChild($i)->getChildren()]);
    }
    return \@expressions;
}

1;
