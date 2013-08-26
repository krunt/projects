package tests;

use 5.014002;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('tests');

1;

