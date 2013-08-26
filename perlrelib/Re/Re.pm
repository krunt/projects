package Re::Re;

use strict;
use warnings;

use Re::Compiler ();

use constant MATCH_NORMAL          => 0;
use constant MATCH_CASEINDEPENDENT => 1;
use constant MATCH_MULTILINE       => 2;
use constant MATCH_SINGLELINE      => 4;

use constant OP_END             => 'E';
use constant OP_BOL             => '^';
use constant OP_EOL             => '$';
use constant OP_ANY             => '.';
use constant OP_ANYOF           => '[';
use constant OP_BRANCH          => '|';
use constant OP_ATOM            => 'A';
use constant OP_STAR            => '*';
use constant OP_PLUS            => '+';
use constant OP_MAYBE           => '?';
use constant OP_ESCAPE          => '\\';
use constant OP_OPEN            => '(';
use constant OP_OPEN_CLUSTER    => '<';
use constant OP_CLOSE           => ')';
use constant OP_CLOSE_CLUSTER   => '>';
use constant OP_BACKREF         => '#';
use constant OP_GOTO            => 'G';
use constant OP_NOTHING         => 'N';
use constant OP_CONTINUE        => 'C';
use constant OP_INIT_COUNTER    => 'I';
use constant OP_COUNTER         => 'M';
use constant OP_CLOSE_COUNTER   => 'c';
use constant OP_RELUCTANTSTAR   => '8';
use constant OP_RELUCTANTPLUS   => '=';
use constant OP_RELUCTANTMAYBE  => '/';
use constant OP_POSIXCLASS      => 'P';
use constant OP_OPEN_LOOKAHEAD  => 'x';
use constant OP_CLOSE_LOOKAHEAD => 'X';
use constant OP_OPEN_ATOMIC     => 'f';
use constant OP_CLOSE_ATOMIC    => 'F';
use constant OP_OPEN_LOOKBEHIND  => 's';
use constant OP_CLOSE_LOOKBEHIND => 'S';

use constant E_ALNUM  => 'w';
use constant E_NALNUM => 'W';
use constant E_BOUND  => 'b';
use constant E_NBOUND => 'B';
use constant E_SPACE  => 's';
use constant E_NSPACE => 'S';
use constant E_DIGIT  => 'd';
use constant E_NDIGIT => 'D';

use constant MAXNODE => 65536;
use constant MAX_PAREN => 16;

sub new {
    my ($class, $pattern, $flags) = @_;

    unless ($pattern) {
        die "There is no pattern";
    }

    my $program = Re::Compiler->new()->compile($pattern);
    return bless {
        program => $program,
        match_flags   => $flags || MATCH_NORMAL(),
        search  => '',
        max_paren => MAX_PAREN(),
        paren_count => $program->parens(),
        paren_starts => [ (-1) x $program->parens() ],
        paren_ends   => [ (-1) x $program->parens() ],
    }, $class;
}

sub match {
    my ($self, $search, $ind) = @_;
    $ind ||= 0;
    $self->assert($self->{program}, "No RE program to run!");
    $self->{search} = $search;

    for my $i ($ind .. length($search)) {
        if ($self->match_at($i)) {
            return 1;
        }
    }
    return 0;
}

sub set_paren_start {
    my ($self, $pindex, $val) = @_;
    if ($pindex < $self->{paren_count}) {
        $self->{paren_starts}[$pindex] = $val;
    }
}

sub set_paren_end {
    my ($self, $pindex, $val) = @_;
    if ($pindex < $self->{paren_count}) {
        $self->{paren_ends}[$pindex] = $val;
    }
}

sub match_at {
    my ($self, $ind) = @_;
    $self->set_paren_start(0, $ind);
    if ((my $idx = $self->match_nodes(0, MAXNODE(), $ind)) != -1) {
        $self->set_paren_end(0, $idx - 1);
        return 1;
    }
    return 0;
}

sub match_nodes {
    my ($self, $from, $to, $idx_start) = @_;
    my $idx = $idx_start;
    my $instr  = $self->{program}->instruction();
    my $node = $from;
MAIN:
    while ($node < $to) {   
        my $opcode = $instr->[$node][Re::Compiler::OPCODE()];
        my $opdata = $instr->[$node][Re::Compiler::OPDATA()];
        my $next   = $node + $instr->[$node][Re::Compiler::OPNEXT()];
        my $opoper = $instr->[$node][Re::Compiler::OPOPER()];

        if ($opcode eq OP_MAYBE() || $opcode eq OP_STAR()) {
            if ((my $idx_new = $self->match_nodes($node + 1, MAXNODE(), $idx)) != -1) {
                return $idx_new;
            }
        } elsif ($opcode eq OP_PLUS) {
            if ((my $idx_new = $self->match_nodes($next, MAXNODE(), $idx)) != -1) {
                return $idx_new;
            }
            $node = $next + $instr->[$next][Re::Compiler::OPNEXT()];
            next;
        } elsif ($opcode eq OP_RELUCTANTMAYBE() || $opcode eq OP_RELUCTANTSTAR()) {
            if ((my $idx_new = $self->match_nodes($next, MAXNODE(), $idx)) != -1) {
                return $idx_new;
            }
            return $self->match_nodes($node + 1, $next, $idx);
        } elsif ($opcode eq OP_RELUCTANTPLUS()) {
            if ((my $idx_new = $self->match_nodes($next 
                    + $instr->[$next][Re::Compiler::OPNEXT()], MAXNODE(), $idx)) != -1) 
            { return $idx_new; }
        } elsif ($opcode eq OP_BRANCH()) {
            my $next_branch;
            do {
                if ((my $idx_new = $self->match_nodes($node + 1, MAXNODE(), $idx)) != -1) {
                    return $idx_new;
                }
                $next_branch = $instr->[$node][Re::Compiler::OPNEXT()];
                $node += $next_branch;
            } while ($next_branch != 0 && $instr->[$node][Re::Compiler::OPCODE()] eq OP_BRANCH());

            return -1;
        } elsif ($opcode eq OP_BOL()) {
            if ($idx != 0) {
                return -1;
            }
        } elsif ($opcode eq OP_EOL()) {
            if ($idx != length($self->{search})) {
                return -1;
            }
        } elsif ($opcode eq OP_ANY()) {
            if ($idx >= length($self->{search})) {
                return -1;
            }
            $idx++;
        } elsif ($opcode eq OP_ANYOF()) {
            if ($idx >= length($self->{search})) {
                return -1;
            }
            my $include_to_interval = $opdata & 1;
            $opdata >>= 1;
            my $found = 0;
            for my $i (0 .. $opdata - 1) {
                my ($min, $max) = map { ord } split //, substr($opoper, 2 * $i, 2);
                my $c = ord(substr($self->{search}, $idx, 1));
                if ($c >= $min && $c <= $max) {
                    $found = 1;
                    last;
                }
            }
            if ((($include_to_interval + $found) & 1) != 0) {
                return -1;
            }
            $idx++;
        } elsif ($opcode eq OP_INIT_COUNTER()) {
            $instr->[$next][Re::Compiler::OPOPER()] = 0;
        } elsif ($opcode eq OP_COUNTER()) {
            my ($c1, $c2) = ($opdata >> 16, $opdata & 0xFFFF);
            if ($opoper < $c1) {
                if ((my $new_idx = $self->match_nodes($node + 1, MAXNODE(), $idx)) != -1) {
                    return $new_idx;
                }
                return -1;
            } elsif ($opoper >= $c1 && ($c2 == 0xFFFF || $opoper <= $c2)) {
                if ((my $new_idx = $self->match_nodes($node + 1, MAXNODE(), $idx)) != -1) {
                    return $new_idx;
                }
            } else {
                return -1;
            }
        } elsif ($opcode eq OP_CLOSE_COUNTER()) {
            $instr->[$next][Re::Compiler::OPOPER()]++;
        } elsif ($opcode eq OP_BACKREF()) {
            my $content = $self->group($opdata);
            if ( length($content) > length($self->{search}) - $idx + 1
                    || $content ne substr($self->{search}, $idx, length($content))) {
                return -1;
            }
            $idx += length($content);
        } elsif ($opcode eq OP_ATOM()) {
            if ($idx >= length($self->{search}) 
                    || length($opoper) > length($self->{search}) - $idx + 1
                    || $opoper ne substr($self->{search}, $idx, length($opoper))) {
                return -1;
            }
            $idx += length($opoper);
        } elsif ($opcode eq OP_ESCAPE()) {
            if ($idx >= length($self->{search})) {
                return -1;
            }
            my $p = $idx == 0 ? 0 : substr($self->{search}, $idx - 1, 1);
            my $c = substr($self->{search}, $idx, 1);
            if ($c =~ /\w/ && (($opdata eq E_BOUND && !$p) 
                                || ($opdata eq E_NBOUND && $p))) {
                return -1;
            }
            if ($opdata eq E_ALNUM) {
                $c !~ /\w/ && return -1;
            } elsif ($opdata eq E_NALNUM) {
                $c !~ /\W/ && return -1;
            } elsif ($opdata eq E_DIGIT) {
                $c !~ /\d/ && return -1;
            } elsif ($opdata eq E_NDIGIT) {
                $c !~ /\D/ && return -1;
            } elsif ($opdata eq E_SPACE) {
                $c !~ /\s/ && return -1;
            } elsif ($opdata eq E_NSPACE) {
                $c !~ /\S/ && return -1;
            }
            $idx++;
        } elsif ($opcode eq OP_OPEN()) {
            $self->set_paren_start($opdata, $idx);
        } elsif ($opcode eq OP_CLOSE()) {
            $self->set_paren_end($opdata, $idx - 1);
        } elsif ($opcode eq OP_OPEN_ATOMIC) {
            if ((my $idx_new = $self->match_nodes($next, MAXNODE(), $idx)) != -1) {
                $idx = $idx_new;
                $node = $opdata;
                next;
            }
            return -1;
        } elsif ($opcode eq OP_CLOSE_ATOMIC) {
            return $idx;
        } elsif ($opcode eq OP_OPEN_CLUSTER || $opcode eq OP_CLOSE_CLUSTER) {
        } elsif ($opcode eq OP_OPEN_LOOKBEHIND) {
            my $midx = $idx - 1;
            while ($midx >= 0) {
                my $idx_new;
                if (($idx_new = $self->match_nodes($next, MAXNODE(), $midx)) != -1 
                            && $idx == $idx_new) 
                { 
                    $node = $opdata;
                    next MAIN; 
                }
                $midx--;
            }
            return -1;
        } elsif ($opcode eq OP_CLOSE_LOOKBEHIND) {
            return $idx;
        } elsif ($opcode eq OP_OPEN_LOOKAHEAD) {
            if ($opdata == 0) {
                # positive
                if ((my $idx_new = $self->match_nodes($next, MAXNODE(), $idx)) != -1) {
                    $node = $opoper;
                    next;
                }
            } else {
                #negative
                if ($self->match_nodes($next, MAXNODE(), $idx) == -1) {
                    $node = $opoper;
                    next;
                }
            }
            return -1;
        } elsif ($opcode eq OP_CLOSE_LOOKAHEAD) {
            return $idx;
        } elsif ($opcode eq OP_NOTHING || $opcode eq OP_GOTO) {
        } elsif ($opcode eq OP_CONTINUE()) {
            $node++;
            next;
        } elsif ($opcode eq OP_END()) {
            return $idx; 
        } else {
            $self->internal_error("Invalid opcode '" . $opcode . "'");
        }

        $node = $next;
    }

    $self->internal_error("Corrupt program");
    return -1;
}

sub substitute {
    my ($self, $string, $replace_string) = @_;
    if ($self->match($string)) {
        my $what = $self->group(0);
        $string =~ s/$what/$replace_string/;
    }
    return $string;
}

sub splitit {
    my ($self, $string) = @_;
    my @result;
    my $prev = 0;
    my ($f, $t);
    while (1) {
        if ($self->match($string, $prev) == 1) {
            $f = $self->{paren_starts}[0];
            $t = $self->{paren_ends}[0];
            if ($prev <= $f - 1) {
                push(@result, substr($string, $prev, $f - $prev));
            }
            if ($self->paren_count() > 1) {
                for my $i (1..$self->paren_count()) {
                    push(@result, $self->group($i));
                }
            }
            $prev = $t + 1;
        } else {
            last;
        }
    }
    if ($t + 1 < length($string)) {
        push(@result, substr($string, $t + 1));
    }
    return @result;
}

sub paren_count {
    my ($self) = @_;
    return $self->{paren_count};
}

sub group {
    my ($self, $ind) = @_;
    if ($ind < $self->{paren_count}) {
        return 
            (($self->{paren_starts}[$ind] <= $self->{paren_ends}[$ind])
            ?  substr($self->{search}, $self->{paren_starts}[$ind], 
                $self->{paren_ends}[$ind] - $self->{paren_starts}[$ind] + 1)
            : "");
    }
    return;
}

sub internal_error { 
    my ($self, $error) = @_;
    die "Internal error: " . $error;
}

sub assert {
    my ($self, $boolean_expression, $assert_text) = @_;
    unless ($boolean_expression) {
        die $assert_text;
    }
}

1;

