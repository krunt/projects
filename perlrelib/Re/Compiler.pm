package Re::Compiler;

use strict;
use warnings;
use Data::Dumper;

use Re::Program ();
use Re::Range   ();

use constant ESC_MASK    => 0xffff0;
use constant ESC_BACKREF => 0xfffff;
use constant ESC_COMPLEX => 0xffffe;
use constant ESC_CLASS   => 0xffffd;

use constant NODE_NORMAL   => 0;
use constant NODE_NULLABLE => 2;
use constant NODE_TOPLEVEL => 4;

use constant OPCODE => 0;
use constant OPDATA => 1;
use constant OPNEXT => 2;
use constant OPOPER => 3;

use constant DEBUG => 0;

sub new {
    my ($class) = @_;
    return bless {
        idx => 0,
        pattern => '',
        parens => 1,
        instruction => [],
    }, $class;
}

sub emit {
    my ($self, $c) = @_;
    $self->assert(@{$self->{instruction}} > 0, "instruction array is empty");
    $self->{instruction}[-1][OPOPER()] .= $c;
}

sub insert_node {
    my ($self, $opcode, $opdata, $index) = @_;
    $self->debug("insert_node: " . $self->{idx} . " " . $opcode . " " . $opdata);
    splice(@{$self->{instruction}}, $index, 0, [ $opcode, $opdata, 0, '' ]);
}

sub node {
    my ($self, $opcode, $opdata) = @_;
    $self->debug("node: " . $self->{idx} . " " . $opcode . " " . $opdata);
    push(@{$self->{instruction}}, [ $opcode, $opdata, 0, '' ]);
    return $#{$self->{instruction}};
}

sub connect_node {
    my ($self, $from, $to) = @_;
    my $next = $self->opnext($from);
    while ($next != 0 && $from < $self->instr_len()) {
        if ($from == $to) {
            $to = $#{$self->{instruction}};
        }
        $from += $next; 
        $next = $self->opnext($from);
    }
    if ($from <= $#{$self->{instruction}}) {
        $self->{instruction}[$from][OPNEXT] = $to - $from;
    }
}

sub compile {
    my ($self, $pattern) = @_;
    $self->{pattern} = $pattern; 
    my $flags = NODE_TOPLEVEL();
    $self->expr(\$flags);
    $self->debug("compile: " . Dumper($self->{instruction}));
    if ($self->{idx} != length($pattern)) {
        if (substr($pattern, $self->{idx}, 1) eq ')') {
            $self->syntax_error("Unmatched right parenthesis");
        }
        $self->syntax_error("Unexpected input remains");
    }
    return Re::Program->new($self->{instruction}, $self->{parens});
}

sub pi {
    my ($self, $inc) = @_;
    $inc ||= 0;
    return substr($self->{pattern}, $self->{idx} + $inc, 1);
}

sub opcode {
    my ($self, $i) = @_;
    return $self->{instruction}[$i][OPCODE()];
}

sub opdata {
    my ($self, $i) = @_;
    return $self->{instruction}[$i][OPDATA()];
}

sub opnext {
    my ($self, $i) = @_;
    return $self->{instruction}[$i][OPNEXT()];
}

sub len {
    my ($self) = @_;
    return length($self->{pattern});
}

sub instr_len {
    my ($self) = @_;
    return scalar @{$self->{instruction}};
}

sub is_closure {
    my ($self, $c) = @_;
    return exists {'{' => 1, '?' => 1, '+' => 1, '*' => 1}->{$c};
}

sub in {
    my ($self, $what, @tokens) = @_;
    return exists { map {$_ => 1} @tokens }->{$what};
}

sub expr {
    my ($self, $flags) = @_;
    my $paren = -1; 
    my $ret = -1;
    my $close_parens = $self->{parens};

    $self->debug("expr: " . $self->{idx});

    if (($$flags & NODE_TOPLEVEL()) == 0 && $self->pi() eq '(') {
        if ($self->{idx} + 2 < $self->len()
                && $self->pi(1) eq '?' 
                && $self->pi(2) eq ':')
        {
            $paren = 2;
            $self->{idx} += 3;
            $ret = $self->node(Re::Re::OP_OPEN_CLUSTER(), 0);
        } elsif ($self->{idx} + 2 < $self->len()
                && $self->pi(1) eq '?' 
                && $self->pi(2) eq '=')
        {
            $paren = 3;
            $self->{idx} += 3;
            $ret = $self->node(Re::Re::OP_OPEN_LOOKAHEAD(), 0);
        } elsif ($self->{idx} + 2 < $self->len()
                && $self->pi(1) eq '?' 
                && $self->pi(2) eq '!')
        {
            $paren = 3;
            $self->{idx} += 3;
            $ret = $self->node(Re::Re::OP_OPEN_LOOKAHEAD(), 1);
        } elsif ($self->{idx} + 2 < $self->len()
                && $self->pi(1) eq '?' 
                && $self->pi(2) eq '>')
        {
            $paren = 4;
            $self->{idx} += 3;
            $ret = $self->node(Re::Re::OP_OPEN_ATOMIC(), 0);
        } elsif ($self->{idx} + 3 < $self->len()
                && $self->pi(1) eq '?' 
                && $self->pi(2) eq '<' && $self->pi(3) eq '=')
        {
            $paren = 5;
            $self->{idx} += 4;
            $ret = $self->node(Re::Re::OP_OPEN_LOOKBEHIND(), 0);
        } else {
            $paren = 1;
            $self->{idx}++;
            $ret = $self->node(Re::Re::OP_OPEN(), $self->{parens}++);
        }
    }

    $$flags &= ~NODE_TOPLEVEL();

    my $open = 0;
    my $branch = $self->branch($flags);
    if ($ret == -1) {
        $ret = $branch;
    } else {
        $self->connect_node($ret, $branch);
    }

    while ($self->{idx} < $self->len() && $self->pi() eq '|') {
        if (! $open) {
            $self->insert_node(Re::Re::OP_BRANCH(), 0, $branch);
            $open = 1;
        }

        $self->{idx}++;
        my $tmp = $self->node(Re::Re::OP_BRANCH(), 0);
        $self->connect_node($branch, $tmp);
        $branch = $tmp;
        $self->branch($flags);
    }

    my $end;
    if ($paren > 0) {
        if ($self->{idx} < $self->len() && $self->pi() eq ')') {
            $self->{idx}++;
        } else {
            $self->syntax_error("Missing closing paren");
        }
        if ($paren == 1) {
            $end = $self->node(Re::Re::OP_CLOSE(), $close_parens);
        } elsif ($paren == 2) {
            $end =  $self->node(Re::Re::OP_OPEN_CLUSTER(), 0);
        } elsif ($paren == 3) {
            $end =  $self->node(Re::Re::OP_CLOSE_LOOKAHEAD(), 0);
            $self->{instruction}[$ret][OPOPER()] = $end + 1;
        } elsif ($paren == 4) {
            $end =  $self->node(Re::Re::OP_CLOSE_ATOMIC(), 0);
            $self->{instruction}[$ret][OPDATA()] = $end + 1;
        } elsif ($paren == 5) {
            $end =  $self->node(Re::Re::OP_CLOSE_LOOKBEHIND(), 0);
            $self->{instruction}[$ret][OPDATA()] = $end + 1;
        }
    } else {
        $end = $self->node(Re::Re::OP_END(), 0);
    }

    $self->assert($ret != -1, "Bad here");

    $self->connect_node($ret, $end);
    my $current = $ret;
    my $next = $self->opnext($current);
    while ($next != 0 && $current < $self->instr_len()) {
        if ($self->opcode($current) eq Re::Re::OP_BRANCH()) {
            $self->connect_node($current + 1, $end);
        }
        $next = $self->opnext($current);
        $current += $next;
    }

    return $ret;
}

sub branch {
    my ($self, $flags) = @_;
    my $nullable = 1;
    my $chain = -1;
    my $ret = -1;

    $self->debug("branch " . $self->{idx});

    while ($self->{idx} < $self->len() && $self->pi() ne '|' && $self->pi() ne ')') 
    {
        my $closure_flags = NODE_NORMAL();
        my $node = $self->closure(\$closure_flags); 
        if ($closure_flags == NODE_NORMAL()) {
            $nullable = 0;
        }

        if ($chain != -1) {
            $self->connect_node($chain, $node);
        }

        $chain = $node;
        if ($ret == -1) {
            $ret = $node;
        }
    }

    if ($ret == -1) {
        $ret = $self->node(Re::Re::OP_NOTHING(), 0);
    }

    if ($nullable) {
        $$flags |= NODE_NULLABLE();
    }

    return $ret;
}

sub closure {
    my ($self, $flags) = @_;

    $self->debug("closure: " . $self->{idx});

    my $terminal_flags = NODE_NORMAL();
    my $ret = $self->terminal(\$terminal_flags);
    $$flags |= $terminal_flags;

    if ($self->{idx} >= $self->len()) {
        return $ret;
    }

    my ($p1, $p2) = (undef, undef);
    my $greedy = 1;
    my $closure_type = $self->pi();
    if ($closure_type eq '?' || $closure_type eq '*') {
        $$flags |= NODE_NULLABLE();
        $self->{idx}++;
    } elsif ($closure_type eq '+') {
        $self->{idx}++;
    } elsif ($closure_type eq '{') {
        $self->{idx}++;
        if ($self->{idx} >= $self->len()) {
            $self->syntax_error("unterminated - {");
        }
        while ($self->{idx} < $self->len() && $self->pi() =~ /^\d$/) {
            $p1 ||= 0;
            $p1 = $p1 * 10 + $self->pi();
            $self->{idx}++;
        }
        if ($self->pi() eq '}') {
            $p2 = $p1;
        } else {
            while ($self->{idx} < $self->len() && ($self->pi() eq ' ' || $self->pi() eq ',')) {
                $self->{idx}++;
            }
            while ($self->{idx} < $self->len() && $self->pi() =~ /^\d$/) {
                $p2 ||= 0;
                $p2 = $p2 * 10 + $self->pi();
                $self->{idx}++;
            }
            unless (defined $p1) {
                $self->syntax_error("there should be {12,12} or {12,}");
            }
            if ($self->pi() ne '}') {
                $self->syntax_error("there is no }");
            }
            if (defined $p2 && $p1 > $p2) {
                $self->syntax_error("p1 > p2");
            }
        }
        #skipping '}'
        $self->{idx}++;
    }

    if ($self->{idx} < $self->len() && $self->pi() eq '?') {
        $self->{idx}++;
        $greedy = 0;
    }

    if ($greedy) {
        my $proc = {
            '?' => sub {
                $self->insert_node( Re::Re::OP_MAYBE(), 0, $ret );
                my $n = $self->node( Re::Re::OP_NOTHING(), 0 );
                $self->connect_node( $ret,     $n );
                $self->connect_node( $ret + 1, $n );
              },
            '*' => sub {
                $self->insert_node( Re::Re::OP_STAR(), 0, $ret );
                $self->connect_node( $ret + 1, $ret );
              },
            '+' => sub {
                $self->insert_node( Re::Re::OP_CONTINUE(), 0, $ret );
                my $n = $self->node( Re::Re::OP_PLUS(), 0 );
                $self->connect_node( $ret + 1, $n );
                $self->connect_node( $n, $ret );
              },
            '{' => sub {
                $p2 ||= 0xFFFF;
                $self->insert_node( Re::Re::OP_COUNTER(), $p1 * (1<<16) + $p2, $ret );
                $self->insert_node( Re::Re::OP_INIT_COUNTER(), 0, $ret );
                my $n = $self->node( Re::Re::OP_CLOSE_COUNTER(), 0 );
                $self->connect_node( $ret, $ret + 1 );
                $self->connect_node( $ret + 2, $n );
                $self->connect_node( $n, $ret + 1 );
              },
        }->{$closure_type};
        if (defined $proc) {
            $proc->();
        }
    } else {
        my $proc = {
            '?' => sub {
                $self->insert_node( Re::Re::OP_RELUCTANTMAYBE(), 0, $ret );
                my $n = $self->node( Re::Re::OP_NOTHING(), 0 );
                $self->connect_node( $ret,     $n );
                $self->connect_node( $ret + 1, $n );
              },
            '*' => sub {
                $self->insert_node( Re::Re::OP_RELUCTANTSTAR(), 0, $ret );
                $self->connect_node( $ret + 1, $ret );
              },
            '+' => sub {
                $self->insert_node( Re::Re::OP_CONTINUE(), 0, $ret );
                my $n = $self->node( Re::Re::OP_RELUCTANTPLUS(), 0 );
                $self->connect_node( $ret + 1, $n );
                $self->connect_node( $n, $ret );
              },
        }->{$closure_type};
        if (defined $proc) {
            $proc->();
        }
    }

    return $ret;
}

sub terminal {
    my ($self, $flags) = @_;

    $self->debug("terminal: " . $self->{idx});

    if ($self->pi() eq Re::Re::OP_ANY()
            || $self->pi() eq Re::Re::OP_BOL()
                || $self->pi() eq Re::Re::OP_EOL())
    { 
        my $x = $self->pi();
        $self->{idx}++;
        return $self->node($x, 0);
    }

    if ($self->pi() eq '[') {
        return $self->character_class();
    }

    if ($self->pi() eq '(') {
        return $self->expr($flags);
    }

    if ($self->pi() eq ')') {
        $self->syntax_error("Unexpected close paren");
    }

    if ($self->pi() eq '|') {
        $self->internal_error("Unexpected '|'");
    }

    if ($self->pi() eq ']') {
        $self->internal_error("Mismatched class");
    }

    if ($self->is_closure($self->pi())) {
        $self->syntax_error("Missing operand to closure");
    }

    if ($self->pi() eq '\\') {
        my $idx_before_escape = $self->{idx};    
        my $escape_type = $self->escape();
        if ($escape_type == ESC_CLASS()) {
            $$flags &= ~NODE_NULLABLE();
            return $self->node(Re::Re::OP_ESCAPE(), $self->pi(-1));
        } elsif ($escape_type == ESC_BACKREF()) {
            return $self->node(Re::Re::OP_BACKREF(), $self->pi(-1));
        }

        $self->{idx} = $idx_before_escape;
    }

    $$flags &= ~NODE_NULLABLE();
    return $self->atom();
}

sub escape {
    my ($self) = @_;

    $self->debug("escape: " . $self->{idx});

    if ($self->pi() ne '\\') {
        $self->internal_error("hey, why are you calling me");
    }
    if ($self->{idx} >= $self->len() - 1) {
        $self->syntax_error("Escape terminates string");
    }
    if ($self->pi(1) =~ /^\d$/) {
        $self->pi(1) < $self->{parens}
            or $self->syntax_error("invalid backref index");
        $self->{idx} += 2;
        return ESC_BACKREF(); 
    }

    $self->{idx} += 2;
    if ($self->pi(-1) eq Re::Re::E_BOUND()
            || $self->pi(-1) eq Re::Re::E_NBOUND())
    { return ESC_COMPLEX; }

    if ($self->pi(-1) eq Re::Re::E_ALNUM()
            || $self->pi(-1) eq Re::Re::E_DIGIT() 
                || $self->pi(-1) eq Re::Re::E_NDIGIT() 
                    || $self->pi(-1) eq Re::Re::E_NALNUM() 
                        || $self->pi(-1) eq Re::Re::E_SPACE() 
                            || $self->pi(-1) eq Re::Re::E_NSPACE()) 
    {
        return ESC_CLASS();
    } 
    return $self->pi(-1);
}

sub atom {
    my ($self) = @_;
    my $ret = $self->node(Re::Re::OP_ATOM(), 0); 
    my $atom_length = 0;

    $self->debug("atom: " . $self->{idx});

    while ($self->{idx} < $self->len()) {
        if ($self->{idx} + 1 < $self->len()) {
            my $c = $self->pi(1);
            if ($self->pi() eq '\\') {
                my $idx_escape = $self->{idx};
                $self->escape();
                if ($self->{idx} < $self->len()) {
                    $c = $self->pi();
                }
                $self->{idx} = $idx_escape;
            }
            if ($self->is_closure($c) && $atom_length) {
                last;
            }
        }
        if ($self->in($self->pi(), ']', '^', '$', '.', '[', '(', ')', '|')) {
            last;
        }
        if ($self->is_closure($self->pi())) {
            unless ($atom_length) { $self->syntax_error("Missing operand to closure"); }
            last;
        }

        my $c;
        if ($self->pi() eq '\\') {
            my $idx_before_escape = $self->{idx};
            $c = $self->escape();
            if (($c & ESC_MASK()) == ESC_MASK()) {
                $self->{idx} = $idx_before_escape;
                last;
            }
        } else {
            $c = $self->pi();
            $self->{idx}++;
        }
        $self->emit($c);
        $atom_length++;
    }

    unless ($atom_length) {
        $self->internal_error("there must be something in atom");
    }

    $self->debug("atom: length=" . $atom_length);

    $self->{instruction}[$ret][OPDATA()] = $atom_length;

    return $ret;
}

sub character_class {
    my ($self) = @_;

    $self->debug("character_class: " . $self->{idx});

    unless ($self->pi() eq '[') {
        $self->internal_error("[ not expected");
    }
    if ($self->{idx} >= $self->len() || $self->pi(1) eq ']') {
        $self->syntax_error("empty or unterminated character class");
    }


    $self->{idx}++;

    my $ret = $self->node(Re::Re::OP_ANYOF(), 0);

    my $range = Re::Range->new();
    my $simple_char;
    my $defined_range = 0;
    my ($range_start, $range_end);
    my $last;
    my $include_to_interval = 1;
    if ($self->pi() eq '^') {
        $include_to_interval = 0;
        $self->{idx}++;
    }
    while ($self->{idx} < $self->len() && $self->pi() ne ']') {
        if ($self->pi() eq '-') {
            if ($defined_range) {
                $self->syntax_error("bad class range");
            }
            $defined_range = 1;
            $range_start = $last;
            $self->{idx}++;
            next;
        } else {
            $simple_char = $self->pi();
        }
        if ($defined_range) {
            $range_end = $simple_char;
            if (ord($range_start) > ord($range_end)) {
                $self->syntax_error("bad character class");
            }
            $range->insert($range_start, $range_end);
            $defined_range = 0;
        } else {
            if ($self->{idx} >= $self->len() || $self->pi() ne '-') {
                $range->insert($simple_char);
            }
            $last = $simple_char;
        }
        $self->{idx}++;
    }
    if ($self->{idx} == $self->len()) {
        $self->syntax_error("Unterminated character class");
    }

    #absorb ']'
    $self->{idx}++;

    $self->{instruction}[$ret][OPDATA()] = $range->len() * 2 + $include_to_interval;
    for my $i (0 .. $range->len() - 1) {
        $self->emit(chr($range->min_range($i)));
        $self->emit(chr($range->max_range($i)));
    }
    return $ret;
}

sub internal_error { 
    my ($self, $error) = @_;
    die "Internal error: " . $error;
}

sub syntax_error { 
    my ($self, $error) = @_;
    die "Syntax error: " . $error;
}

sub assert {
    my ($self, $boolean_expression, $assert_text) = @_;
    unless ($boolean_expression) {
        die $assert_text;
    }
}

sub debug {
    my ($self, $string) = @_;
    if (DEBUG()) {
        print(STDERR $string."\n");
    }
}

1;

