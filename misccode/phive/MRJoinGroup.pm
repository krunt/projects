package MRJoinGroup;

use strict;
use warnings;

use base qw(MRStep);
use Asserter;
use Command;

use constant JOIN_REDUCE_STEP => 'steps/group_joinreduce.pl';

sub execute {
    my ($self, $src_tables) = @_;

    my $src_data = Utils::bloat(JOIN_REDUCE_STEP);
    $self->patch_where(\$src_data);

    my ($queryA, $queryB) =  $self->{query}->from()->queries();
    my ($labelA, $labelB) = ($queryA->name(), $queryB->name());
    $src_data =~ s/__A__/$labelA/g;
    $src_data =~ s/__B__/$labelB/g;
    $src_data =~ s/__INPUT_A__/$queryA->get_fieldlist()/eg;
    $src_data =~ s/__INPUT_B__/$queryB->get_fieldlist()/eg;

    my @aggregate_labels 
            = $self->{query}->select()->{self}->aggregate_function_names;
    Asserter::assert(scalar @aggregate_labels > 0, "There is no aggregates");

    my $group_by_key = $self->{query}->groupby()->statement_string();

    my $other = join ", ", $self->{query}->select()->{self}->sum_labels();
    $other = ", " . $other if ($other);

    my $starting_descriptor_index = 3;
    my $desc = '';
    my $output = "";
    my %uniq_fields;
    my @dst_tables;
    my @result;
    my @functions;
    my $i = 0;
    for my $label (sort {(Utils::split_id($a))[2] 
                            cmp (Utils::split_id($b))[2]} @aggregate_labels) {
        my ($function_name, $table_name, $field_name) = Utils::split_id($label);
        my $key = Utils::combine_id($table_name, $field_name);
        if (@functions && exists $functions[-1]{$key}) {
            push(@{$functions[-1]{$key}}, $function_name);
        }
        exists $uniq_fields{$key} and next;
        push(@functions, {$key => [$table_name, $field_name, $function_name]});
        $uniq_fields{$key}++;
        my $descriptor_name = '$desriptor_' . $field_name;
        $desc .= "open my $descriptor_name, "
                        . "'>&$starting_descriptor_index' or die \$!;\n";
        $output .=  "\t\tprint {$descriptor_name} " 
                         . qq{"$group_by_key\\r$key", $key$other;\n};
        push(@result, [$self->{pcontext}->get_last_table(), substr($key, 1)]);
        push(@dst_tables, $self->{pcontext}->get_last_table_and_increment());
        $starting_descriptor_index++;
    }
    $src_data =~ s/__DESC__/$desc/g;
    $src_data =~ s/__OUTPUT__/$output/g;
    $src_data =~ s/__LEFTJOIN__/
                ($self->{query}->from()->{self}->join_type() == PConstants::TOK_LEFTOUTERJOIN)
                ? ", []" : ""/eg;

    my $dstname = $self->destfile(MRStep::REDUCE);
    Utils::write_to_file($dstname, \$src_data);

    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
            type => 'REDUCE',
            script => $dstname,
            src  => $self->get_src_tablenames(),
            dst  => \@dst_tables,
            write_to_several_descriptors => 1,
        })
    );

    return [\@result, \@functions];
}

1;
