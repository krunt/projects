package GroupJoinAlgorithm;

use strict;
use warnings;

use base qw(Algorithm);

use MRJoinGroup;
use MRGroupReduce;
use MRGroupMap_2;
use MRGroupCombine;

sub execute { 
    my $self = shift;

    my $result = MRJoinGroup->new($self->{query}, 
                        $self->{pcontext}, $self->{join_field})->execute();
    my $dst_tables = $result->[0];
    my $fields     = $result->[1];

    my $index = 0;
    my @destinations;
    for my $table_name (@$dst_tables) {
        my $functions = $fields->[$index++];
        my $table_name_after_reduce 
            = MRGroupReduce->new($self->{query}, $self->{pcontext}, 
                        $self->{join_field})->execute([$table_name->[0]], $functions);
        my $table_name_after_map_2
            = MRGroupMap_2->new($self->{query}, $self->{pcontext}, 
                        $self->{join_field})->execute($table_name_after_reduce, 
                                                            $table_name->[1], $functions);
        push(@destinations, $table_name_after_map_2->[0]);
    }

    return MRGroupCombine->new($self->{query}, $self->{pcontext},
                         $self->{join_field})->execute(\@destinations, $fields);
}


1;
