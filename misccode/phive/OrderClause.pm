package OrderClause;

use strict;
use warnings;

use constant KW_ASC  => 0;
use constant KW_DESC => 1;

sub new {
    my ($class, $statements, $direction, $pcontext) = @_;
    return bless { 
        direction => $direction,
        statements => $statements,
        pcontext   => $pcontext,
    }, $class;
}

sub pcontext() {
    return shift()->{pcontext};
}

sub statements {
    return @{shift()->{statements}};
}

sub direction {
    return shift()->{direction};
}

sub get_order_part {
    my ($self, $select_part) = @_;
    my $item = $select_part->get_item_by_alias($self->{statements}[0][1]);
    if ($item) {
        return 'sprintf("%030d", ' 
                . ($self->{direction} == KW_DESC() ? sprintf("%d -", 1<<30) : "" )
                . ' (' . $item->code() . ' || 0))';
    }
    return;
}

1;

