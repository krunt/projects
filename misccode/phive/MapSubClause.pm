package MapSubClause;

use strict;
use warnings;

sub new {
    my ($class, $params, $pcontext) = @_;
    my $self = bless $params, $class;

    my $current_alias = $pcontext->current_alias(); 
    $pcontext->db()->get($current_alias)->set_fields( 
            { map {$_ => 1} @{$params->{aliases}} }
    );

    return $self;
}

sub script_name {
    return Utils::strip(shift->{script_name});
}

sub filters {
    return @{shift()->{filters}};
}

sub params {
    return Utils::strip(shift()->{params} || '');
}

sub field_names {
    my ($self) = @_;
    return @{$self->{aliases}};
}

sub field_codes {
    my ($self) = @_;
    return ();
}

1;
