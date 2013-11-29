package Function;

use strict; 
use warnings;

sub new {
    my ($class, $name, $isAggregate, 
            $number_of_arguments, $code, $short_name) = @_;
    return bless {
        function_name => $name, 
        is_aggregate  => $isAggregate, 
        num_arguments => $number_of_arguments,
        code          => $code,
        short_name    => $short_name,
    }, $class;
}

sub name {
    return shift->{function_name};
}
sub is_aggregate {
    return shift->{is_aggregate};
}
sub num_arguments {
    return shift->{num_arguments};
}
sub code {
    return shift->{code};
}
sub short_name {
    return shift->{short_name} || "__undef__";
}

sub generate_code {
    my $self = shift;
    return if $self->is_aggregate();

    my $func_code = $self->code();
    for my $i (0 .. $self->num_arguments() - 1) {
        my $argument = shift;
        defined $argument
            or die "not enough arguments to function " . $self->name();
            
        $func_code =~ s/__$i\__/$argument/;
    }
    die "invalid usage of function " . $self->name() . ". There "
            . "must be " . $self->num_arguments() . " arguments." 
        if (@_);

    return $func_code;
}


1;
