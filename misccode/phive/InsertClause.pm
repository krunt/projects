package InsertClause;

use strict;
use warnings;

use Utils;
use PConfig;

sub new {
    my ($class, $tablename, $is_temp, $pcontext) = @_;

    $pcontext->db()->get($pcontext->current_alias())
                     ->set_raw_name($tablename);

	my $need_to_finish = 0;
	$tablename = Utils::strip($tablename);
	if ($tablename =~ /^finished_/) {
		$tablename =~ s/^finished_//;
		$need_to_finish = 1;
	}
    return bless {
        tablename => '$job_root/' . $tablename,
        is_temp   => $is_temp,
		finish    => $need_to_finish,
    }, $class;
}

sub tablename {
	my ($self) = @_;
	return $self->{tablename};
}

sub is_finish {
	my ($self) = @_;
	return $self->{finish};
}

sub is_temporary {
    return shift->{is_temp};
}

1;
