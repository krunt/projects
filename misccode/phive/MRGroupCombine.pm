package MRGroupCombine;

use strict;
use warnings;

use base qw(MRStep);
use Asserter;
use Command;
use Utils;
use Functions;

use constant MAP_GROUP_COMBINER => 'steps/group_combiner.pl';

sub execute {
    my ($self, $src_tables, $fields) = @_;

    my $src_data = Utils::bloat(MAP_GROUP_COMBINER);

    my @aggregate_labels 
            = $self->{query}->select()->{self}->aggregate_function_names;
    Asserter::assert(scalar @aggregate_labels > 0, "There is no aggregates");

    my $output = $self->combine_to_join($self->{query}->get_codefieldlist());

    my $parent = $self->{pcontext}{parent};
    my $order_part = ""; 
    if ($parent && $parent->orderby()) {
        $order_part = $parent->orderby()->get_order_part($self->query()->select());
        $output =~ s/([,)])/, $order_part$1/;
    }

    my $functions_db = Functions->new();

    my $input = '';
    my @check;
    my %uniq_fields;
    my $index = 0;
    $fields = [ sort {(keys%$a)[0] cmp (keys%$b)[0]} @$fields ];
    for my $label (
            sort {
                substr($a, index($a, '$'))
                cmp
                substr($b, index($b, '$'))
            } 
            @aggregate_labels) {
        my ($function_name, $table_name, $field_name) = Utils::split_id($label);
        my $key = Utils::combine_id(substr($table_name, 1), $field_name);
        if (!exists $uniq_fields{$key}) {
            $uniq_fields{$key}++;
            my @functions = @{$fields->[$index++]{'$' . $key}};
            splice(@functions, 0, 2);
            $input .=  qq|\tmy (| 
                        . join(", ", 
                          map {
                            '$' . $key . Utils::combine_id(
                                    $functions_db->{uc($_)}->short_mkey())
                          } sort @functions)
                        . qq|) = \@{\$hash{$key}};\n|;
            push(@check, qq|defined \$hash{$key}|);
        }
        $output =~ s/\Q$label\E/b($function_name, $key)/eg;
    }

    #adding keys from prev_key
    $input .=  "\t" . 'my (' 
                .  $self->{query}->groupby()->statement_declaration_string()
                . ') = split /\r/, $prev_key;' . "\n";

    my $check = "\t" . join(' && ', @check) . " or do { %hash = (); return; };";

    $src_data =~ s/__SUBKEY__/$parent->orderby() ? "undef, " : ""/ge;
    $src_data =~ s/__INPUT__/$input/g;
    $src_data =~ s/__CHECK__/$check/g;
    $src_data =~ s/__OUTPUT__/$output/g;

    my $dstname = $self->destfile(MRStep::REDUCE);
    Utils::write_to_file($dstname, \$src_data);

    my $dst_table = $self->get_query_dst_table();
    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => 'REDUCE',
	        script => $dstname,
	        src  => $src_tables,
	        dst  => [$dst_table],
            params => ' -jobcount 1 ' . ($parent->orderby() ? ' -subkey ' : ''),
        })
    );
    $self->create_select_distinct_step($dst_table)
        if $self->{query}->is_distinct();

    return [$self->{query}->insert()->tablename()];
}

{
    my $functions_db = Functions->new();
	sub b {
	    my ($function_name, $key) = @_;
	    return '$' . $key . 
            Utils::combine_id($functions_db->{uc($function_name)}->short_mkey());
	}
}

1;

