package Driver;

use lib ".";

use strict;
use warnings;
use ParseDriver;
use Asserter;
use PContext;
use SemanticAnalyzer;
use PlanGenerator;

sub new {
    my ($class, $is_local_mode) = @_;
    my $pcontext = PContext->new($is_local_mode);
    return bless { context => $pcontext }, $class;
}

sub run {
    my ($self, $qfname) = @_;

    my ($string_tree, $tree) = @{ParseDriver->new()->parse($qfname)};
    Asserter::assert($string_tree && $string_tree !~ /error/i,
            "parse error. check query syntax");
    $self->{context}->reset();
    my $query_tree = SemanticAnalyzer->new()->parse($tree, $self->{context});
    PlanGenerator->new()->gen_plan($query_tree, $self->{context});
}

1;
