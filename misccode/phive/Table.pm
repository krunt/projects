package Table;

use strict;
use warnings;

sub new {
    my ($class, $table_name) = @_;
    return bless {
        table_name => $table_name,
        raw_name   => '',
        fields     => {},

        from_tables => [], #from where I get data

        query      => '', #pointer to query node
    }, $class;
}

sub tableName {
    return shift->{table_name};
}

sub set_raw_name {
    my ($self, $what) = @_;
    $self->{raw_name} = $what;
}

sub set_fields {
    my ($self, $fields) = @_;
    $self->{fields} = $fields;
}

sub set_query {
    my ($self, $query) = @_;
    $self->{query} = $query;
}

sub set_from_tables {
    my ($self) = shift;
    @{$self->{from_tables}} = @_;
}

sub from_tables {
    return @{shift->{from_tables}};
}

sub is_field_here {
    my ($self, $what) = @_;
    return exists($self->{fields}{$what});
}

1;
