package SelectSubClause;

use strict;
use warnings;
use Utils;

sub new {
    my ($class, $params, $pcontext) = @_;
    my $self = bless {
        %$params,
        pcontext => $pcontext,
    }, $class;

    my $current_alias = $pcontext->current_alias(); 
    $pcontext->db()->get($current_alias)->set_fields( 
        { map {$_ => 1} $self->field_names() }
    );

    return $self;
}

sub is_distinct {
    return shift()->{is_distinct};
}

sub field_names {
    my ($self) = @_;
    return map {$_->field_name()} @{$self->{items}};
}

sub field_codes {
    my ($self) = @_;
    return map {$_->code()} @{$self->{items}};
}

sub aggregate_function_names {
    my ($self) = @_;
    my %hash = map { $_->aggregate_functions() } @{$self->{items}};
    return keys %hash;
}

sub sum_labels {
    my ($self) = @_;

    my %additional_fields;
    for my $aggregate_ffield ($self->aggregate_function_names()) {
        my ($function_name, $table_name, $field_name) = Utils::split_id($aggregate_ffield);
        $function_name =~ /^SUM/i
            and $additional_fields{Utils::combine_id($table_name, $field_name)}++;
    }
    return sort keys %additional_fields;
}

1;
