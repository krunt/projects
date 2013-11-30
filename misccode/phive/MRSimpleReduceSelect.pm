package MRSimpleReduceSelect;

use strict;
use warnings;

use base qw(MRStep);

use Utils ();
use Command;

sub execute {
    my $self = shift;

    my $src = $self->{query}->select()->{self}->script_name();
    my $src_data = Utils::bloat($src);
    my $dstname = $self->destfile(MRStep::REDUCE);
    $self->patch_filters(\$src_data);
    Utils::write_to_file($dstname, \$src_data);

    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => 'REDUCE',
	        script => $dstname,
	        src  => [$self->{query}->from()->tablename()  ],
	        dst  => [$self->{query}->insert()->tablename()],
            params => $self->{query}->select()->{self}->params(),
        })
    );
    return [$self->{query}->insert()->tablename()];
}

1;
