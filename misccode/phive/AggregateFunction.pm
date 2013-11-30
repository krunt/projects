package AggregateFunction;

use strict; 
use warnings;

our($AUTOLOAD);

sub new {
    my ($class, 
			$name, 
			$init_reduce,
			$flush_reduce,
			$ag_reduce,
			$fun_map2,
            $short_mkey,
			$need_reduce,
	) = @_;

	if (!defined $need_reduce) {
		$need_reduce = 1;
	}

    return bless {
        function_name => $name, 
		init_reduce => $init_reduce,
		flush_reduce => $flush_reduce,
		ag_reduce => $ag_reduce,
		fun_map2 => $fun_map2,
        short_mkey => $short_mkey,
		need_reduce => $need_reduce,
    }, $class;
}

sub AUTOLOAD {
	my ($self) = @_;
	(my $method = $AUTOLOAD) =~ s/.*:://;
    if ($method eq 'DESTROY') 
    { return; }

	my $x;
	if (exists $self->{$method}) {
		$x = sub { return $self->{$method}; };
	}
	die "there is no suitable method" unless($x);
	goto &$x;
}

sub is_aggregate {
    return 1;
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
