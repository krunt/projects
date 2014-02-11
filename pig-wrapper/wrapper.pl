#!/usr/bin/perl -w
use strict;

use POSIX ();
use Carp ();
use MIME::Base64 ();

# map
# in - always from 3 - 3,5,7,9...
# outputs - 4,6,8,10,...

# reduce 
# in - only 1 possible == 0
# outputs - 4,6,8,10,...

sub sys_check {
    unless (defined $_[0]) {
        Carp::confess($!);
    }
}

my ($cmd, $type, $num_inputs, $num_outputs, 
    $tmp_inpmap, $date_length) = @ARGV;

$tmp_inpmap = MIME::Base64::decode_base64($tmp_inpmap);

# open for simplicity further
if ($num_outputs == 1) {
    POSIX::dup2(1, 3);
}

my @inpfd;
for (1..$num_inputs) {
    my ($fd1, $fd2);
    pipe($fd1, $fd2);
    push(@inpfd, [$fd1, $fd2]);
}

my $pid = fork();
unless (defined $pid) { 
    die $!;
}

if (!$pid) {
    # child 

    for my $fd (@inpfd) {
        close($fd->[1]);
    }

    # prepare input descriptors
    my %fdoutmap;
    for my $i (1..$num_outputs) {
        my $fd = 2 + $i;
        $fdoutmap{$fd} = $fd;
    }

    my $out_max_fd = ($num_outputs == 1 ? 2 : 3 + $num_outputs - 1); 
    my $max_fd = $out_max_fd + 2 * $num_inputs + 2 * $num_outputs;
    for my $i (1..$num_inputs) {
        my $fd = 1 + $i * 2;

        # need to move
        if ($fd <= $max_fd) {

            # so move it
            POSIX::dup2($fd, $max_fd + 1);
            POSIX::close($fd);

            if (exists $fdoutmap{$fd}) {
                $fdoutmap{$fd} = $max_fd + 1;
            }

            ++$max_fd;
        }

        sys_check(POSIX::dup2(fileno($inpfd[$i - 1][0]), $fd));
    }

    # prepare output descriptors
    my %actual_map = reverse %fdoutmap;
    my @outfd = sort {$a <=> $b} keys %fdoutmap;
    for (my $i = 0, my $dest_fd = 4; $i < @outfd; ++$i, $dest_fd += 2) {
        my $nextfd_to_assign = $outfd[$i];
        my $actual_fd = $fdoutmap{$nextfd_to_assign};

        # we are done
        if ($actual_fd == $dest_fd) {
            next;
        }

        # if busy by not us, move it
        if (exists $actual_map{$dest_fd}) {

            my $fd_to_assign = $actual_map{$dest_fd};

            sys_check(POSIX::dup2($dest_fd, $max_fd + 1));
            sys_check(POSIX::close($dest_fd));

            $fdoutmap{$fd_to_assign} = $max_fd + 1;
            $actual_map{$max_fd + 1} = $fd_to_assign;

            delete $actual_map{$dest_fd};

            ++$max_fd;
        }

        $fdoutmap{$nextfd_to_assign} = $dest_fd;
        delete $actual_map{$actual_fd};

        sys_check(POSIX::dup2($actual_fd, $dest_fd));
        sys_check(POSIX::close($actual_fd));
    }

    # clear unused fds
    for my $fd (3..$max_fd) {

        # if input descriptor 
        if (($fd & 1) && $fd > 3 + ($num_inputs - 1) * 2) {
            POSIX::close($fd); # check later
        }

        if (!($fd & 1) && $fd > 4 + ($num_outputs - 1) * 2) {
            POSIX::close($fd); # check later
        }
    }

    if ($type eq 'map' && $num_outputs) {
        sys_check(POSIX::dup2(4, 1));
    }

    if ($type eq 'reduce' && $num_inputs) {
        sys_check(POSIX::dup2(3, 0));
    }

    exec $cmd 
        or die $!;
}

# closing unused
for my $fd (@inpfd) {
    close($fd->[0]);
}

my @inputfd;
for my $i (0..$#inpfd) {
    my $input = $inpfd[$i];

    # if tmp input, then 1 table
    if (vec($tmp_inpmap, $i, 1)) {
        push(@inputfd, $input->[1]);
    } else {
        for (1..$date_length) {
            push(@inputfd, $input->[1]);
        }
    }
}

if ($type eq 'map') {
    
    my $write_fd;
    my $re = qr{^\d+$};
    while (<STDIN>) {
    
        if ($_ =~ $re) {
            $write_fd = $inputfd[int($_)];
            next;
        }
    
        # for now just remove tabulation in subkey
        # TODO

        s/\t\t/\t/;
    
        print {$write_fd} $_;
    }

} else {

    #optimization for reduce

    while (<STDIN>) {
        print {$inputfd[0]} $_;
    }

}


for my $fd (@inpfd) {
    close($fd->[1]);
}

waitpid($pid, 0);

if ($?) {
    my ( $sig, $ec ) = ( POSIX::WTERMSIG($?), POSIX::WEXITSTATUS($?) );
    my $msg = "mapreduce child $pid died "
      . ( POSIX::WIFSIGNALED($?) ? "due to signal $sig" : "with code $ec" );
    die $msg;
}
