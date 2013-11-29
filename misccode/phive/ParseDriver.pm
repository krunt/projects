package ParseDriver;

use strict;
use warnings;

use ASTNode;

sub new { 
    return bless {}, shift();
}

sub parse {
    my ($self, $qfname) = @_;
    my $string_tree = `./parser $qfname`;
    return [ $string_tree, $self->_parse($string_tree) ];
}

sub _parse {
    my ($self, $text) = @_;

    my @tokens = map {($_ eq '0' || $_)? ($_) : ()}
                    map { split /([()])/, $_ }
                        split / /, $text;

    my $idx = 1;
    my ($tree) = @{ _Build_tree($idx, \@tokens) };
    return $tree;
}

sub _Build_tree {
    my ($idx, $tokens) = @_;

    my $node_root = ASTNode->new($tokens->[$idx]);
    my $i = $idx + 1;
    for (; $i < @$tokens; ++$i) {
        if ($tokens->[$i] eq '(') {
            my ($tree, $increment) = @{_Build_tree($i + 1, $tokens)};
            $node_root->add_child($tree); 
            $i += $increment;
        } elsif ($tokens->[$i] eq ')') {
            last;
        } else {
            $node_root->add_child(ASTNode->new($tokens->[$i])); 
        }
    }
    return [$node_root, $i - $idx + 1];
}

1;
