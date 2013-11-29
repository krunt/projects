package Command;

use strict;
use warnings;

use PConfig;

sub new {
    my ($class, $params) = @_;
    return bless {
        mr     => lc($params->{type}),
        script => $params->{script} || '',
        args   => $params->{arguments} || [],
        src    => $params->{src} || [],
        dst    => $params->{dst} || [],
        files  => $params->{files} || [],
        params => $params->{params} || PConfig::MR_PARAMS(),
        write_to_several_descriptors => $params->{write_to_several_descriptors} || 0,
    }, $class;
}

sub string_for_local {
    my $self = shift;
    my $index = 3;

    return 
        sprintf("cat %s | %s perl %s %s\n",
           join(" ", @{$self->{src}}),
           $self->{mr} eq 'reduce' ? 'sort |' : '',
           $self->script_string(),
           $self->{write_to_several_descriptors}
                ? join("", map { ' ' . $index++ . '>' . $_ }  @{$self->{dst}})
                : join("", map { ' >' . $_ }  @{$self->{dst}}));
}

sub string_for_bash {
    my $self = shift;

    return 
        sprintf("mr -%s %s %s %s %s %s\n",
           $self->{mr},
           $self->script_string(),
           join("", map { ' -src '   . $_ }  @{$self->{src}}),
           join("", map { ' -dst '   . $_ }  @{$self->{dst}}),
           join("", map { ' -file ' . $_ }  @{$self->{files}}),
           $self->{params});
}

sub string_for_mpppoc {
    my $self = shift;

    return 
        sprintf("%s %s %s %s %s %s\n",
           $self->{mr},
           $self->script_string(),
           join("", map { ' -src '   . $_ }  @{$self->{src}}),
           join("", map { ' -dst '   . $_ }  @{$self->{dst}}),
           join("", map { ' -file ' . $_ }  @{$self->{files}}),
           $self->{params});
}

sub script_string {
    my $self = shift;
    if (@{$self->{args}}) {
        return sprintf('./%s %s', $self->{script}, 
                    join(" ", map {"'" . b($_) . "'"} @{$self->{args}}));
    }
    return "./" . $self->{script};
}

sub b {
    my $what = shift;
    $what =~ s/'/\\'/g;
    return $what;
}


1;
