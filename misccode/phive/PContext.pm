package PContext;

use strict;
use warnings;
use DB;
use PConfig;

sub new {
    my ($class, $local_mode) = @_;

	if (!defined $local_mode) 
	{ $local_mode = 1; }

    my $self = {
        DB            => DB->new(),
        current_alias => '',
        map_last_one     => 1,
        reduce_last_one  => 1,
        last_table_index => 1,

		local_mode       => $local_mode,
    };
    return bless $self, $class;
}

sub reset {
    my ($self) = @_;
    $self->{DB} = DB->new();
    $self->{current_alias} = '';
}

sub db {
    return shift->{DB};
}

sub current_alias {
    return shift->{current_alias};
}

sub is_local_mode {
	return shift->{local_mode};
}

sub set_current_alias {
    my ($self, $what) = @_;
    my $old_one = $self->{current_alias};
    $self->{current_alias} = $what;
    return $old_one;
}

sub prefix_src {
    return PConfig::PREFIX_SRC;
}

sub get_last_table {
    my ($self) = @_;
    return $self->table_prefix() . 'A' . $self->{last_table_index};
}

sub increment_last_table {
    my ($self) = @_;
    $self->{last_table_index}++;
}

sub get_last_table_and_increment {
    my ($self) = @_;
    return $self->table_prefix() . 'A' . $self->{last_table_index}++;
}

sub table_prefix {
	my ($self) = @_;	
	if ($self->{local_mode}) {
		return $self->{local_table_prefix} || "data/";
	}
	return $self->{global_table_prefix} || '$tmp/';
}

1;
