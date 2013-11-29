package MRJoinReduce;

use strict;
use warnings;

use base qw(MRStep);

use Command;
use constant JOIN_STEP => 'steps/join_reduce.pl';

sub execute {
    my ($self, $src_tables) = @_;

    my $src_data = Utils::bloat(JOIN_STEP);
    $self->patch_where(\$src_data);

    my ($queryA, $queryB) =  $self->{query}->from()->queries();

    my ($labelA, $labelB) = ($queryA->name(), $queryB->name());
    $src_data =~ s/__A__/$labelA/g;
    $src_data =~ s/__B__/$labelB/g;
    $src_data =~ s/__INPUT_A__/$queryA->get_fieldlist()/eg;
    $src_data =~ s/__INPUT_B__/$queryB->get_fieldlist()/eg;
    $src_data =~ s/__OUTPUT__/
         $self->combine_to_join($self->{query}->get_codefieldlist())/eg;
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
	        dst  => [$self->{query}->insert()->tablename()],
        })
    );

    return $self->{query}->insert()->tablename();
}


1;
