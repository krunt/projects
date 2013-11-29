package MRSimpleSelect;

use strict;
use warnings;

use base qw(MRStep);

use Utils ();
use Command;

use constant SELECT_STEP => 'steps/select_map.pl';

sub execute {
    my ($self, $src_tables) = @_;

    my $src_data = Utils::bloat(SELECT_STEP);
    $self->patch_where(\$src_data);
    my $input_part = $self->{query}->from()->query()->get_fieldlist();
    if ($self->query()->orderby()) {
        $input_part =~ s/([,)])/, undef$1/;
    }
    $self->patch_input(\$src_data, $input_part);

    my $output_part = $self->combine_to_join($self->{query}->get_codefieldlist());
    if ($self->query()->limit()) {
        $output_part =~ s/([,)])/, undef$1/;
        $output_part = "if (\$rec_count < " . $self->query()->limit()->limit() . ") {\n"
            . $output_part
            . "}";
    }
    $self->patch_output(\$src_data, $output_part);

    my $dstname = $self->destfile(($self->query()->orderby() ? MRStep::REDUCE : MRStep::MAP));
    Utils::write_to_file($dstname, \$src_data);

    my $dst_table = $self->get_query_dst_table();
    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => ($self->query()->orderby() ? 'REDUCE' : 'MAP'),
	        script => $dstname,
	        src  => [$self->{query}->from()->tablename()],
	        dst  => [$dst_table],
            params => ($self->query()->orderby() ? ' -jobcount 1 -subkey ' : ''),
        })
    );
    $self->create_select_distinct_step($dst_table)
        if $self->{query}->is_distinct();

    return [$self->{query}->insert()->tablename()];
}

1;
