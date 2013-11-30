package QueryClauseParser;

use strict;
use warnings;

use PConstants;
use Asserter;
use QueryClause;
use Table;

use FromClauseParser;
use SelectClauseParser;
use GroupClauseParser;
use InsertClauseParser;
use WhereClauseParser;
use OrderClauseParser;
use OrderClauseParser;
use LimitClauseParser;

sub new { return bless {}, shift; }

sub parse {
    my ($self, $raw_statement, $alias, $pcontext) = @_;

    Asserter::assert_equals($raw_statement->getName(), 
            PConstants::TOK_QUERY, 
            "it is not query clause in QueryClauseParser");

    my $query = QueryClause->new($alias);
    $pcontext->db()->add_table(Table->new($alias));
    $pcontext->db()->get($alias)->set_query($query);
    my $old_current_alias = $pcontext->set_current_alias($alias);

    for my $child ($raw_statement->getChildren()) {
        if ($child->getName() == PConstants::TOK_INSERT) {
            for my $subchild ($child->getChildren()) {
                +{
                    PConstants::TOK_DESTINATION() 
                        => sub {
                            shift->set_insert(InsertClauseParser->new()->parse(
                                        $subchild->getChild(0), $pcontext));
                        },
                    PConstants::TOK_SELECT() 
                        => sub {
                            shift->set_select(SelectClauseParser->new()->parse(
                                        $subchild, 0, $pcontext));
                        },
                    PConstants::TOK_SELECTDI() 
                        => sub {
                            shift->set_select(SelectClauseParser->new()->parse(
                                        $subchild, 1, $pcontext));
                        },
                    PConstants::TOK_WHERE() 
                        => sub {
                            shift->set_where(WhereClauseParser->new()->parse(
                                        $subchild->getChild(0), $pcontext));
                        },
                    PConstants::TOK_GROUPBY() 
                        => sub {
                            shift->set_groupby(GroupClauseParser->new()->parse(
                                        $subchild, $pcontext));
                        },
                    PConstants::TOK_ORDERBY() 
                        => sub {
                            shift->set_orderby(OrderClauseParser->new()->parse(
                                        $subchild, $pcontext));
                        },
                    PConstants::TOK_LIMIT() 
                        => sub {
                            shift->set_limit(LimitClauseParser->new()->parse(
                                        $subchild, $pcontext));
                        },
                }->{$subchild->getName()}->($query);
            }
        } elsif ($child->getName() == PConstants::TOK_FROM) {
            $query->set_from(FromClauseParser->new()->parse(
                        $child->getChild(0), $pcontext));
        } else {
            Asserter::assert(0, 
                "Unknown query parse type" . $child->getName());
        }
    }

    $pcontext->set_current_alias($old_current_alias);

    return $query;
}

1;
