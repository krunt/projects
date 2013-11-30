package MRStep;

use strict;
use warnings;

use constant MAP => 1;
use constant REDUCE => 2;
use constant DSELECT => 'steps/select_distinct.pl';

use Utils;

sub new {
    my ($class, $query, $pcontext, $join_field) = @_;
    return bless {
        pcontext => $pcontext,
        query    => $query,
        join_field => $join_field,
    }, $class;
}

sub query {
    return shift->{query};
}

sub execute {
    die "abstract";
}

sub get_src_tablenames {
    my $self = shift;
    return [ 
            map { $_->insert()->tablename() } 
                $self->{query}->from()->queries()
        ];
}

sub join_prefix_string {
    my $self = shift;
    return '' unless $self->{join_field};
    return ('$' . Utils::combine_id($self->get_src_join_table(), $self->{join_field}), 
            "'" . $self->{query}->name() . "'");
}

sub get_src_join_table {
    my ($self) = @_;
    my @query_names = map { $_->name() }
                $self->{query}->from()->queries();
    for my $name (@query_names) {
        my $table = $self->{pcontext}->db()->get($name);
        if ($table->is_field_here($self->{join_field})) {
            return $name;
        }
    }
    return '';
}

sub combine_to_join {
    my ($self, $rest) = @_;

    my $result = '';
    unless ($self->join_prefix_string()) {
        $result .= $rest;
    } else {
        $result .= join(", ", $self->join_prefix_string()) . ", " . $rest;
    }

    if ($self->{query}->is_distinct()) {
        return 
            "BEGIN { use Digest::MD5 qw(md5_hex); }; \n"
            . 'my $md5hex = md5_hex(join("\t", ' . $result . "));\n"
            . 'print $md5hex, join("\r", ' . $result . ");";
    }

    return 'print ' . $result . ";\n";
}

sub destfile {
    my ($self, $what) = @_;
    if ($what == MAP) {
        return $self->{pcontext}->prefix_src() . sprintf('map_step_%d.pl', 
                 $self->{pcontext}->{map_last_one}++);
    } elsif ($what == REDUCE) {
        return $self->{pcontext}->prefix_src() . sprintf('reduce_step_%d.pl', 
                $self->{pcontext}->{reduce_last_one}++);
    } else {
        die "isn't expected";
    }
}

sub create_select_distinct_step {
    my ($self, $src_table) = @_;

    my $src_data = Utils::bloat(DSELECT);
    my $dstname = $self->destfile(MRStep::REDUCE);
    Utils::write_to_file($dstname, \$src_data);

    $self->{pcontext}->{algomake}->add_command(
        Command->new(
        {
	        type => 'REDUCE',
	        script => $dstname,
	        src  => [$src_table  ],
	        dst  => [$self->{query}->insert()->tablename()],
        })
    );
}

sub get_query_dst_table {
    my ($self) = @_; 
    if ($self->{query}->is_distinct()) {
        return $self->{pcontext}->get_last_table_and_increment();
    } else {
        return $self->{query}->insert()->tablename();
    }
}

sub patch_input {
    my ($self, $data, $bywhat) = @_;
    $$data =~ s/__INPUT__/$bywhat/g;
}

sub patch_output {
    my ($self, $data, $bywhat) = @_;
    $$data =~ s/__OUTPUT__/$bywhat/g;
}

sub patch_where {
    my ($self, $data) = @_;
    my $bywhat = $self->{query}->where() ? $self->{query}->where()->code() : '';
    $$data =~ s/__WHERE__/$bywhat/g;
}

sub patch_filters {
    my ($self, $data) = @_;

    my @filters = $self->{query}->select()->{self}->filters();
    my %filters = map { uc($_->name()) => [$_->params()] } @filters;

	my $result = '';
    for my $line (split /\n/, $$data) {
        if ($line =~ /^#(\w+)\s*(.*)/) {
            my ($what, $rest) = (uc($1), $2);
            next unless ($rest);

            if ($what eq 'FLUSH' && exists $filters{$what}) {
                my $hkey =  'join("\t", ' . join(", ", map {qq|\$record{'$_'}|}
                                grep {!/^hits$/} $self->{query}->select()->{self}->field_names())
                            . ')';
                $rest =~ s/\$hash\{\}\+\+/\$hash\{$hkey\}\+\+/;
            }

            if (exists $filters{$what}) {
                for my $index (0..$#{$filters{$what}}) {
                    $rest =~ s/__$index\__/$filters{$what}[$index]/g;
                }
                $result .= "\t" . $rest . "\n";
            }
        } else {
            if ($line =~ /^\s*print\s*$/) {
                $result .= sprintf("\tprint(%s);\n",
                           join(", ", 
                            map {qq|\$record{'$_'}|}
                            $self->{query}->select()->{self}->field_names()))
                        if (not exists $filters{'FLUSH'});
            } else {
                $result .= $line . "\n";
            }
        }
    }

    $$data = $result;
}

1;
