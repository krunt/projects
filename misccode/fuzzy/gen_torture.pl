#!/usr/bin/perl -w
use strict;

my @fields = 'a'..'l';

print '#include "fuzzy.h"', "\n";

print "class Dragon { private:\n";

print join "\n", map {"double " . $_ . "_;"} @fields;
print "\npublic:\n";
print "\nDragon() : \n";
print join ",", map {$_ . "_(15)"} @fields;
print "\n{}\n";

print join "\n", map {"double " . $_ . "() const { return $_\_; };"} @fields;
print "};\n";

print 'int main() {', "\n";

my $pattern = "
        switch (YYYY) {
            case 10: DDDD
            case 20: DDDD
            case 30: DDDD
            default: return 100
        }";

my $res = $pattern;
for my $i (0..$#fields) {
    my $field = $fields[$i];
    $res =~ s/YYYY/$field/g;
    $res =~ s/(case (\d+): )DDDD/$i != $#fields ? "$1$pattern" : "$1" . "return " . ($2*10)/ge;
}

print 'NodeParser<Dragon> node_parser;', "\n";
for (@fields) {
    print "node_parser.register_accessor(\"$_\", &Dragon::$_);\n";
}

$res =~ s/(?!\z)$/\\/mg;

print "Dragon dragon;\n";
print "const char *weight_format = \"$res\";", "\n";
print "Node<Dragon> *dragon_structure = node_parser.parse(weight_format);\n";
print "double result;\n";
print "for (int i = 0; i < 100; ++i) {\n";
print "result = construct_fuzzy_weigher(dragon_structure)(&dragon);\n";
print "}\n";
print "printf(\"result = %f\\n\", result);\n";
print 'return 0; }', "\n";


