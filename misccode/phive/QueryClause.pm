package QueryClause;

use strict;
use warnings;
use Utils;

sub new {
    my ($class, $query_name) = @_;
    return bless {
         name     => $query_name,
        'select'  => '',
         where    => '',
         insert   => '',
         groupby  => '',
         from     => '',
         orderby  => '',
         limit    => '',
    }, $class;
}

sub get_fieldlist {
    my ($self) = @_;
    return join ", ", map {"\$" . Utils::combine_id($self->{name}, $_)} 
                     $self->{'select'}->field_names();
}

sub get_codefieldlist {
    my ($self) = @_;
    my $output = "";
	if ($self->insert() && $self->insert()->is_finish()) {
		my @names = $self->{select}->field_names();
		my $i = 0;
    	$output = join ", ",  
			map { 
				"pair('" . $names[$i++] . "', " . $_ . ')'
			} $self->{'select'}->field_codes();
	} else {
    	$output = join ", ",  $self->{'select'}->field_codes();
	}
    if (scalar $self->{select}->field_names() <= 1) {
        $output .= ", ''";
    }
    return $output;
}

sub set_select {
    my ($self) = shift;
    $self->{'select'} = shift();
}

sub set_where {
    my ($self) = shift;
    $self->{'where'} = shift();
}

sub set_insert {
    my ($self) = shift;
    $self->{'insert'} = shift();
}

sub set_groupby {
    my ($self) = shift;
    $self->{'groupby'} = shift();
}

sub set_from {
    my ($self) = shift;
    $self->{'from'} = shift();
}

sub set_orderby {
    my ($self) = shift;
    $self->{'orderby'} = shift();
}

sub set_limit {
    my ($self) = shift;
    $self->{'limit'} = shift();
}

sub set_name {
    my ($self) = shift;
    $self->{'name'} = shift();
}

sub name {
    return shift()->{'name'};
}

sub select {
    return shift()->{'select'};
}

sub where {
    return shift()->{'where'};
}

sub insert {
    return shift()->{'insert'};
}

sub from {
    return shift()->{'from'};
}

sub groupby {
    return shift()->{'groupby'};
}

sub limit {
    return shift()->{'limit'};
}

sub orderby {
    return shift()->{'orderby'};
}

sub is_distinct {
    return shift()->select()->{self}->is_distinct();
}

1;
