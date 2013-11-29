package SelectClauseParser;

use strict;
use warnings;

use SelectClause;
use SelectItem;
use PConstants;
use Asserter;
use Utils;
use FilterItem;

sub new {
    return bless {}, shift;
}

sub parse {
    my ($self, $raw_statement, $distinct, $pcontext) = @_;

    Asserter::assert_equals($raw_statement->getChild(0)->getName(), 
                    PConstants::TOK_SELEXPR, "invalid select statement");

    my $next_one = $raw_statement->getChild(0)->getChild(0);
    if ($next_one->getName() == PConstants::TOK_TRANSFORM) {
        #it is a map clause here
        Asserter::assert_equals($next_one->getChild(1)->getName(), 
                    PConstants::TOK_ALIASLIST, 
                    "invalid SELECT MAP/REDUCE statement");

        my $what_is_it = lc($raw_statement->getChild(0)->getChild(1)->getText());

        my $script_name = $next_one->getChild(0)->getText();
        my @aliases = map {$_->getText()} $next_one->getChild(1)->getChildren();

        my @filters; 
        if ($next_one->size() >= 3 
                && $next_one->getChild(2)->getName() == PConstants::TOK_FILTERLIST) 
        {
            #year we got here filters
            for my $filter_item ($next_one->getChild(2)->getChildren()) {
                Asserter::assert_equals($filter_item->getName(),
                    PConstants::TOK_FILTERITEM, "it is not FilterItem statement");
                Asserter::assert($filter_item->size() >= 1, "FilterItem is not empty");
                push(@filters, FilterItem->new(
                            [map {$_->getText()} $filter_item->getChildren()]));
            }
        }

        my $last_index = $next_one->size() - 1;
        my $params = '';
        if ($next_one->size() >= 3 
                && $next_one->getChild($last_index)->getName() == PConstants::TOK_PARAMS) 
        {
            #year we got here params
            $params = $next_one->getChild($last_index)->getChild(0)->getText();
        }

        return SelectClause->new(
                    $what_is_it eq 'map' ? SelectClause::MAP()
                                         : SelectClause::REDUCE(),
                    {
                        script_name => $script_name, 
                        aliases     => \@aliases,
                        filters     => \@filters,
                        params      => do {$params =~ tr/_/ /; $params},
                    },
                    $pcontext,
                );

    } else {
        my @items;
        for my $child ($raw_statement->getChildren()) {
            Asserter::assert_equals($child->getName(), 
                    PConstants::TOK_SELEXPR, "invalid select field statement");
            push(@items, SelectItem->new($child, $pcontext));
        }

        return SelectClause->new(
                    SelectClause::SELECT(),
                    {
                        items       => \@items,
                        is_distinct => $distinct,
                    },
                    $pcontext,
                );
    }
}

1;
