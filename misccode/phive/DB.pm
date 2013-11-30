package DB;

use strict;
use warnings;
use Asserter;

sub new {
    return bless {}, shift;
}

sub add_table {
    my ($self, $table) = @_;

    Asserter::assert(! exists($self->{$table->tableName()}),
        "Table name: " . $table->tableName() . " already exists in DB");

    $self->{$table->tableName()} = $table;
}

sub get {
    my ($self, $what) = @_;
    Asserter::assert(exists($self->{$what}),
        "Table name: " . $what . " does not exist in DB");
    return $self->{$what};
}

1;
