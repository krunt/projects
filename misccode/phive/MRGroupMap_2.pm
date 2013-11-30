package MRGroupMap_2;

use strict;
use warnings;

use base qw(MRStep);

use Utils ();
use Command;

use constant MAP_STEP_2 => 'steps/group_map_2.pl';

sub execute {
    my ($self, $src_table, $arguments, $functions) = @_;

    my $src_data = Utils::bloat(MAP_STEP_2);
    my $dstname = $self->destfile(MRStep::MAP);

    my $functions_db = Functions->new();

    my $process = "";
    my $key = (keys %$functions)[0];
    my @functions = @{$functions->{$key}};
    my ($table_name, $field_name) = splice(@functions, 0, 2);
    for my $function (sort @functions) {
        $process .= $functions_db->{uc($function)}->fun_map2();
    }

    my $input = join ", ", map {'$' . $functions_db->{uc($_)}->short_mkey()} 
                        sort @functions;

    my $output = 'print $k, $label, @$v{qw(' 
        . join(" ", map {$functions_db->{uc($_)}->short_mkey()} sort @functions)
        . ')};';

    $src_data =~ s/__INPUT__/$input/;
    $src_data =~ s/__PROCESS__/$process/;
    $src_data =~ s/__OUTPUT__/$output/;

    Utils::write_to_file($dstname, \$src_data);

    my $dst_table = $self->{pcontext}->get_last_table_and_increment();
    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => 'MAP',
	        script => $dstname,
	        arguments => [$arguments],
	        src  => $src_table,
            dst  => [$dst_table],
            params => ' -jobcount 1 ',
        })
    );
    return [$dst_table];
}

1;
