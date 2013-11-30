package GroupClause;

use strict;
use warnings;
use base qw(BaseClause);

use Utils;

use PConfig;

sub new {
    my ($class, $statements, $pcontext) = @_;
    my $self = bless {
        statements => $statements,
        pcontext   => $pcontext,
    }, $class;

    $self->check();

    return $self;
}

sub pcontext() {
    return shift()->{pcontext};
}

sub statements {
    return @{shift()->{statements}};
}

sub statement_string {
    return join "\\r",
                map {'$' . Utils::combine_id($_->[0], $_->[1])} shift()->statements();
}

sub statement_declaration_string {
    return join ", ",
                map {'$' . Utils::combine_id($_->[0], $_->[1])} shift()->statements();
}

sub check {
    my ($self) = @_;

    for my $statement (@{$self->{statements}}) {
        $self->check_colref($statement, 'GROUP BY');
    }
}

1;
