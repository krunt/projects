package BaseClause;

use strict;
use warnings;
use Carp qw(croak);

use Utils;

sub check_colref {
    my ($self, $colref, $what) = @_;

    my $current_alias = $self->pcontext()->current_alias();
    my $current_table = $self->pcontext()->db()->get($current_alias);

    my ($table_name, $field_name) = @{$colref};

    not $table_name 
        and ($current_table->from_tables() > 1) #here is join
            and croak "can't be $what fields from join without table_name prefix";

    unless ($table_name) {
        ($table_name) = $current_table->from_tables();
    }
     
    my $good = 0;
    for my $from_tablename ($current_table->from_tables()) {
        next if $from_tablename ne $table_name;

        $self->pcontext()->db()->get($from_tablename)->is_field_here($field_name)
            and do { $good = 1; last; };
    }


    croak "invalid statement " . $table_name . "." . $field_name . " in $what "
                       . "statement" unless $good;
}

sub get_from_colref {
    my ($self, $raw_statement) = @_;

    my ($table_name, $field_name) 
            = map { $_->getText() } $raw_statement->getChildren();

    unless ($field_name) {
        $field_name = $table_name;
        undef $table_name;
    }
    return [$table_name, $field_name];
}

sub generate_from_colref {
    my ($self, $colref) = @_;

    my $current_alias = $self->pcontext()->current_alias();
    my $current_table = $self->pcontext()->db()->get($current_alias);

    my ($table_name, $field_name) = @{$colref};

    unless ($table_name) {
        ($table_name) = $current_table->from_tables();
    }

    return "\$" . Utils::combine_id($table_name, $field_name);
}

1;
