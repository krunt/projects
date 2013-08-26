#!/usr/bin/perl 

use strict;
use warnings;

use constant HOST => 'localhost';
use constant PORT => 49999;

use constant NUMBER_OF_REQUESTS => 100;

use IO::Socket ();
use Time::HiRes ();

my $begin = Time::HiRes::time();
for (1..NUMBER_OF_REQUESTS()) {
    my $s = IO::Socket::INET->new(PeerAddr => HOST(), PeerPort => PORT());
    $s->send(  "GET /calculate?script=sample/2.lua HTTP/1.1\n"
         . "Host: localhost\r\n\r\n");
    my $buf;
    $s->recv($buf, (16 << 20));
    select(undef, undef, undef, 0.001);
}
my $end = Time::HiRes::time();
printf("%2.2f\n", $end - $begin);
