package SSH::Socket;

use strict;
use warnings;

use POSIX ();

use constant MAX_PACKET_LEN => 262144;

sub new {
    my ($class, $session) = @_;
    return bless {
        session => $session,
        fds => [],
        timeout => undef,
        exception => sub {},
        data      => sub {},
        reset_in_buffer => 1,
    }, $class;
}

sub add {
    my ($self, $fd) = @_;
    push(@{$self->{fds}}, $fd);
}

sub process {
    my ($self, $read) = @_;
    my $rin = '';
    my $win = '';
    for my $fd (@{$self->{fds}}) {
        vec($rin, fileno($fd), 1) = 1;
        vec($win, fileno($fd), 1) = 1;
    }
    if ($read) {
        $win = undef;
    } else {
        $rin = undef;
    }

    my $nfound;
    if (($nfound = select($rin, $win, undef, $self->{timeout})) <= 0) {
        $self->{exception}->($self->{session}, "select error: $!");
        return;
    }

    $self->{session}->log("select returned with success: $nfound");

    if ($read && $self->{reset_in_buffer}) { 
        $self->{session}{in_buffer} = ''; 
    }

    # hack here
    for my $fd (@{$self->{fds}}[0]) {
        if ($rin && vec($rin, fileno($fd), 1)) {
            my $read_buf = '';
            while (1) {
                my $buf;
                my $rc = sysread($fd, $buf, 1024);

                $self->{session}->log("sysread returned: " . ($rc||""));

                if (! defined $rc && $! != POSIX::EWOULDBLOCK()) {
                    $self->{exception}->($self->{session}, "read error: $!");
                }
                if (defined $rc && $rc == 0) {
                    $self->{session}{session_state} 
                        = SSH::Session::SSH_SESSION_STATE_DISCONNECTED();
                    $self->{session}->log("implicitly disconnected" . ($rc||""));
                }
                unless ($rc) {
                    last;
                }
                $read_buf .= $buf;
            }

            $self->{session}->log("read " . $read_buf);

            $self->{session}{in_buffer} .= $read_buf;

            if (length($self->{session}{in_buffer}) > MAX_PACKET_LEN) {
                $self->{exception}->($self->{session}, 
                        sprintf("big packet: %d", length($self->{session}{in_buffer})));
                return;
            }

            $self->{data}->($self->{session});
            $self->{session}->handle_connection();
        } elsif ($win && vec($win, fileno($fd), 1)) {
            my $buf = $self->{session}{out_buffer};
            my $i = 0;
            my $rest = length($buf);
            while ($rest) {
                my $rc = syswrite($fd, $buf, $rest, $i);

                $self->{session}->log("syswrite returned: " . ($rc||""));

                if (! defined $rc && $! != POSIX::EWOULDBLOCK()) {
                    $self->{exception}->($self->{session}, "write error: $!");
                }
                unless ($rc) {
                    last;
                }
                $rest -= $rc;
                $i += $rc;
            }
        }
    }

    return 1;
}

sub data_callback {
    my ($self, $f) = @_;
    if ($f) {
        $self->{data} = $f;
    }
    return $self->{data};
}

sub exception_callback {
    my ($self, $f) = @_;
    if ($f) {
        $self->{exception} = $f;
    }
    return $self->{exception};
}

sub timeout {
    my ($self, $timeout) = @_;
    $self->{timeout} = $timeout;
}

sub reset_buffer {
    my ($self, $rs) = @_;
    if (defined $rs) {
        $self->{reset_in_buffer} = $rs;
    }
}

1;

