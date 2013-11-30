package PlanGenerator;

use strict;
use warnings;

use FromClause;
use AlgorithmFactory;
use Asserter;
use Algomake;

sub new {
    my ($class) = @_;
    return bless {}, $class;
}

sub gen_plan {
    my ($self, $query_tree, $pcontext) = @_;

    #prepare pcontext here
    $pcontext->{algomake} = Algomake->new();

    $self->do_work($query_tree, $pcontext);

    return $pcontext->{algomake}->make($pcontext->is_local_mode());
}

sub do_work {
    my ($self, $query_tree, $pcontext, $join_field, $parent) = @_;

    my $first_query;
    for my $query ($query_tree->from()->queries()) {
        my $field_to_join;
        if ($query_tree->from()->type() == FromClause::JOIN) {
            $field_to_join = $query_tree->from()->self()
                    ->get_join_field($query->name());
            Asserter::assert(defined $field_to_join);
        }
        $self->do_work($query, $pcontext, $field_to_join, $query_tree);
    }

    $pcontext->{parent} = $parent;
    my $algorithm = AlgorithmFactory->new()->create($query_tree, 
                                            $pcontext, $join_field);
    $algorithm->execute();
}


1;
