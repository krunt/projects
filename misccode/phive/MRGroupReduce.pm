package MRGroupReduce;

use strict;
use warnings;

use base qw(MRStep);

use Utils ();
use Command;

use constant REDUCE_STEP => 'steps/group_reduce.pl';

sub execute {
    my ($self, $src_table, $functions) = @_;

    my $src_data = Utils::bloat(REDUCE_STEP);
    my $dstname = $self->destfile(MRStep::REDUCE);

    my $functions_db = Functions->new();

    my $init = ""; 
    my $in_flush = "";
    my $ag_step = "";
    my $key = (keys %$functions)[0];
    my @functions = @{$functions->{$key}};
    my ($table_name, $field_name) = splice(@functions, 0, 2);
    for my $function (sort @functions) {
        $init .= $functions_db->{uc($function)}->init_reduce();
        $in_flush .=  $functions_db->{uc($function)}->flush_reduce();
        $ag_step .= $functions_db->{uc($function)}->ag_reduce();
    }

    my $output = 'print $k, @$v{qw(' 
        . join(" ", map {$functions_db->{uc($_)}->short_mkey()} sort @functions)
        . ')};';

    $src_data =~ s/__INIT_STEP__/$init/;
    $src_data =~ s/__IN_FLUSH__/$in_flush/;
    $src_data =~ s/__AG_STEP__/$ag_step/;
    $src_data =~ s/__OUTPUT__/$output/;

    Utils::write_to_file($dstname, \$src_data);

    my $dst_table = $self->{pcontext}->get_last_table_and_increment();
    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => 'REDUCE',
	        script => $dstname,
	        src  => $src_table,
	        dst  => [$dst_table],
        })
    );
    return [$dst_table];
}

1;
