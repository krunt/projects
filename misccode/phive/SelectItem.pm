package SelectItem;

use strict;
use warnings;

use PConstants;
use Functions;

use base qw(BaseClause);

sub new {
    my ($class, $raw_statement, $pcontext) = @_;
    my $self = bless {
        raw_statement => $raw_statement,
        code          =>  '',
        pcontext      => $pcontext,
        functions     => Functions->new(),
        aggregate_functions => {},
        item_alias    =>  '',
        function_name => '',
    }, $class;

    if ($raw_statement->size() > 1) {
        $self->{item_alias} = $raw_statement->getChild(1)->getText();
    }
    unless ($self->{item_alias}) {
        $raw_statement->size() == 1
            and $raw_statement->getChild(0)->getName() == PConstants::TOK_COLREF
                or die "There is no alias for non-colref select item";

        my $qchild = $raw_statement->getChild(0);
        $self->{item_alias} = $qchild->size() == 2 
                                ? $qchild->getChild(1)->getText()
                                : $qchild->getChild(0)->getText();

        Asserter::assert($self->{item_alias}, "item alias must be defined");
    }

    $self->{code} = $self->generate_code($raw_statement->getChild(0));

    return $self;
}

sub pcontext {
    return shift()->{pcontext};
}

sub aggregate_functions {
    return %{shift()->{aggregate_functions}};
}

sub function_name {
    return shift()->{function_name};
}

sub field_name {
    return shift()->{item_alias};
}

sub code {
    return shift()->{code};
}

sub generate_code {
    my ($self, $raw_statement) = @_;

    if ($raw_statement->getName() == PConstants::TOK_COLREF()) {
        my $colref = $self->get_from_colref($raw_statement);  
        $self->check_colref($colref, "SELECT");
        return $self->generate_from_colref($colref);
    } elsif (    $raw_statement->getName() == PConstants::TOK_FUNCTION
              || $raw_statement->getName() == PConstants::TOK_FUNCTIONDI
            ) {
        Asserter::assert($raw_statement->size() >= 1, 
                    "Invalid function statement");

        my $function_name = lc($raw_statement->getChild(0)->getText());
        my $distinct = ($raw_statement->getName() == PConstants::TOK_FUNCTIONDI);

        my $function = $self->{functions}->get($function_name, $distinct); 

        if ($function->is_aggregate()) {
            Asserter::assert($raw_statement->size() == 2
                                &&
                             $raw_statement->getChild(1)->getName()
                                == PConstants::TOK_COLREF,
                            "In aggregate functions only colref is allowed");
            my $label = Utils::combine_id($raw_statement->getChild(0)->getText() 
                        . ($distinct == 1 ? 1 : 0)) 
                        .  $self->generate_code($raw_statement->getChild(1));

            
            #collecting aggregate functions
            $self->{aggregate_functions}{$label} = 1;

            return $label;
        }

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
