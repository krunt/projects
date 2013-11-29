package WhereClause;

use strict;
use warnings;
use base qw(BaseClause);
use PConstants;
use Asserter;
use Functions;

sub new {
    my ($class, $raw_statement, $pcontext) = @_;
    my $self = bless {
        raw_statement => $raw_statement,
        code          => '',
        pcontext   => $pcontext,
        functions  => Functions->new(),
    }, $class;

    $self->{code} = $self->generate_code($raw_statement);
    if ($self->{code}) {
        $self->{code} .= " or next;";
    }

    return $self;
}

sub pcontext() {
    return shift()->{pcontext};
}

sub code {
    return shift()->{code};
}

sub generate_code {
    my ($self, $raw_statement) = @_;
    
    if ($raw_statement->getName() == PConstants::TOK_COLREF()) 
    {
        my $colref = $self->get_from_colref($raw_statement);  
        $self->check_colref($colref, "WHERE");
        return $self->generate_from_colref($colref);
    } elsif ($raw_statement->getName() == PConstants::TOK_FUNCTION
              || $raw_statement->getName() == PConstants::TOK_FUNCTIONDI
            ) {
        Asserter::assert($raw_statement->size() >= 1, 
                    "Invalid function statement");

        my $function_name = $raw_statement->getChild(0)->getText();
        my $distinct = ($raw_statement->getName() == PConstants::TOK_FUNCTIONDI);

        my $function = $self->{functions}->get($function_name, $distinct); 

        die "Aggregate function is not allowed in where section"
                if $function->is_aggregate();

        my @arguments;
        for my $i (1 .. $raw_statement->size() - 1) {
            push(@arguments, $self->generate_code( 
                        $raw_statement->getChild($i)));
        }

        return $function->generate_code(@arguments);
         
    } else {
        #terminal
        if ($raw_statement->size() == 0) {
            return $raw_statement->getText();
        }

        #it is operator
        if ($raw_statement->size() == 2) {
            my $operator_name 
                = {AND => '&&', OR => '||', NOT => '!', '<>' => 'ne', '=' => 'eq'}
                    ->{$raw_statement->getText()} || $raw_statement->getText();
            return 
                "(" 
                .  $self->generate_code($raw_statement->getChild(0))
                . " " 
                .  $operator_name
                . " "
                .  $self->generate_code($raw_statement->getChild(1))
                . ")";
        }

        Asserter::assert(0, "Unknown operator " . $raw_statement->getText());
    }
}

1;
