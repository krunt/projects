package SSH::Session;

use strict;
use warnings;

use Socket                     ();
use Fcntl                      ();
use SSH::Socket                ();
use List::Util                 ();
use SSH::Cryptor               ();
use SSH::Packet::FieldIterator ();
use SSH::Packet                ();
use Data::Dumper               ();
use Math::BigInt               ();
use Encode                     ();
use Digest::SHA                ();
use bytes;

use constant SSH_SESSION_STATE_NONE             => 0;
use constant SSH_SESSION_STATE_CONNECTING       => 1;
use constant SSH_SESSION_STATE_SOCKET_CONNECTED => 2;
use constant SSH_SESSION_STATE_BANNER_RECEIVED  => 3;
use constant SSH_SESSION_STATE_INITIAL_KEX      => 4;
use constant SSH_SESSION_STATE_KEXINIT_RECEIVED => 5;
use constant SSH_SESSION_STATE_DH               => 6;
use constant SSH_SESSION_STATE_AUTHENTICATING   => 7;
use constant SSH_SESSION_STATE_AUTHENTICATED    => 8;
use constant SSH_SESSION_STATE_ERROR            => 9;
use constant SSH_SESSION_STATE_DISCONNECTED     => 10;

use constant SSH_LOG_NOLOG    => 0;
use constant SSH_LOG_RARE     => 1;
use constant SSH_LOG_PROTOCOL => 2;
use constant SSH_LOG_PACKET   => 3;
use constant SSH_LOG_ALL      => 4;

use constant CLIENTBANNER1 => "SSH-1.5-myssh-0.0.1";
use constant CLIENTBANNER2 => "SSH-2.0-myssh-0.0.1";

use constant PACKET_STATE_INIT       => 0;
use constant PACKET_STATE_SIZEREAD   => 1;
use constant PACKET_STATE_PROCESSING => 2;

use constant DH_STATE_INIT         => 0;
use constant DH_STATE_INIT_SENT    => 1;
use constant DH_STATE_NEWKEYS_SENT => 2;
use constant DH_STATE_FINISHED     => 3;

use constant SERVICE_INIT   => 0;
use constant SERVICE_ACCEPT => 1;

use constant CHANNEL_OPENING => 0;
use constant CHANNEL_OPENED => 1;
use constant CHANNEL_CLOSING => 2;
use constant CHANNEL_CLOSED => 3;

use constant SSH2_MSG_DISCONNECT                => 1;
use constant SSH2_MSG_IGNORE                    => 2;
use constant SSH2_MSG_UNIMPLEMENTED             => 3;
use constant SSH2_MSG_DEBUG                     => 4;
use constant SSH2_MSG_SERVICE_REQUEST           => 5;
use constant SSH2_MSG_SERVICE_ACCEPT            => 6;
use constant SSH2_MSG_KEXINIT                   => 20;
use constant SSH2_MSG_NEWKEYS                   => 21;
use constant SSH2_MSG_KEXDH_INIT                => 30;
use constant SSH2_MSG_KEXDH_REPLY               => 31;
use constant SSH2_MSG_KEX_DH_GEX_REQUEST_OLD    => 30;
use constant SSH2_MSG_KEX_DH_GEX_GROUP          => 31;
use constant SSH2_MSG_KEX_DH_GEX_INIT           => 32;
use constant SSH2_MSG_KEX_DH_GEX_REPLY          => 33;
use constant SSH2_MSG_KEX_DH_GEX_REQUEST        => 34;
use constant SSH2_MSG_USERAUTH_REQUEST          => 50;
use constant SSH2_MSG_USERAUTH_FAILURE          => 51;
use constant SSH2_MSG_USERAUTH_SUCCESS          => 52;
use constant SSH2_MSG_USERAUTH_BANNER           => 53;
use constant SSH2_MSG_USERAUTH_PK_OK            => 60;
use constant SSH2_MSG_USERAUTH_PASSWD_CHANGEREQ => 60;
use constant SSH2_MSG_USERAUTH_INFO_REQUEST     => 60;
use constant SSH2_MSG_USERAUTH_INFO_RESPONSE    => 61;
use constant SSH2_MSG_GLOBAL_REQUEST            => 80;
use constant SSH2_MSG_REQUEST_SUCCESS           => 81;
use constant SSH2_MSG_REQUEST_FAILURE           => 82;
use constant SSH2_MSG_CHANNEL_OPEN              => 90;
use constant SSH2_MSG_CHANNEL_OPEN_CONFIRMATION => 91;
use constant SSH2_MSG_CHANNEL_OPEN_FAILURE      => 92;
use constant SSH2_MSG_CHANNEL_WINDOW_ADJUST     => 93;
use constant SSH2_MSG_CHANNEL_DATA              => 94;
use constant SSH2_MSG_CHANNEL_EXTENDED_DATA     => 95;
use constant SSH2_MSG_CHANNEL_EOF               => 96;
use constant SSH2_MSG_CHANNEL_CLOSE             => 97;
use constant SSH2_MSG_CHANNEL_REQUEST           => 98;
use constant SSH2_MSG_CHANNEL_SUCCESS           => 99;
use constant SSH2_MSG_CHANNEL_FAILURE           => 100;

use constant DEFAULT_PACKET_HANDLERS => {
    SSH2_MSG_DISCONNECT() => \&_handle_disconnect,
    SSH2_MSG_KEXINIT() => \&_handle_kexinit,
    SSH2_MSG_KEXDH_REPLY() => \&_handle_kex_dh_reply,
    SSH2_MSG_NEWKEYS() => \&_handle_msg_newkeys,
    SSH2_MSG_USERAUTH_SUCCESS() => \&_handle_userauth_success,
    SSH2_MSG_USERAUTH_FAILURE() => \&_handle_userauth_failure,
    SSH2_MSG_UNIMPLEMENTED() => \&_handle_unimplemented,
    SSH2_MSG_SERVICE_ACCEPT() => \&_handle_service_accept,
    SSH2_MSG_CHANNEL_OPEN_CONFIRMATION() => \&_handle_channel_confirmation,
    SSH2_MSG_CHANNEL_OPEN_FAILURE() => \&_handle_channel_failure,
    SSH2_MSG_CHANNEL_EOF() => \&_handle_channel_eof,
    SSH2_MSG_CHANNEL_CLOSE() => \&_handle_channel_closed,
    SSH2_MSG_CHANNEL_WINDOW_ADJUST() => sub {},
};

use constant CHANNEL_STATUS => 0;
use constant CHANNEL_NUMBER => 1;
use constant CHANNEL_EOF => 2;

sub new {
    my ($class, $args) = @_;
    my $self = bless {
        host => $args->{host} || "localhost",
        user => $args->{user} || "root",
        port => $args->{port} || 22,
        session_state => SSH_SESSION_STATE_NONE(),
        session_id => undef,
        socket_obj => undef,

        version => 2,

        packet_state => PACKET_STATE_INIT(),
        dh_state => DH_STATE_INIT(),

        in_buffer => '',
        out_buffer => '',
        temp_in_buffer => '',

        in_packet  => {},
        out_packet => {},

        cur_crypto => undef,

        channel_state => [ ([CHANNEL_CLOSED(), 0, 0 ]) x 100 ],

        log_verbosity => SSH_LOG_ALL(),
    }, $class;

    $self->_dh_init();

    return $self;
}

sub connect {
    my ($self) = @_;

    my $iaddr = gethostbyname($self->{host});
    my $sin_addr = Socket::sockaddr_in($self->{port}, $iaddr);

    my $fd;
    unless (socket($fd, Socket::PF_INET(), Socket::SOCK_STREAM(), Socket::IPPROTO_TCP())) {
        $self->log("socket() error: $!", SSH_LOG_PROTOCOL());
        return;
    }

    unless (connect($fd, $sin_addr)) {
        $self->log("connect() error: $!", SSH_LOG_PROTOCOL());
        return;
    }

    # set socket nonblocking
    fcntl($fd, Fcntl::F_SETFL(), fcntl($fd, Fcntl::F_GETFL(), 0) | Fcntl::O_NONBLOCK());

    $self->{socket_obj} = SSH::Socket->new($self);
    $self->{socket_obj}->add($fd);

    $self->{session_state} = SSH_SESSION_STATE_SOCKET_CONNECTED();
    $self->{socket_obj}->data_callback(\&_handle_banner);
    $self->{socket_obj}->exception_callback(\&_handle_exception);

    $self->_handle_events();

    return $self->{session_state} == SSH_SESSION_STATE_AUTHENTICATING();
}

sub auth {
    my ($self, $args) = @_;
    my $auth_type = $args->{type} || "publickey";

    $self->_ask_service("ssh-userauth")
        or return 0;

    if ($auth_type eq 'password') {
        $self->_send_packet(
                "b;str;str;str;bool;str;",
                [
                    SSH2_MSG_USERAUTH_REQUEST(),
                    Encode::encode('utf8', $args->{user}),
                    "ssh-connection",
                    "password",
                    0,
                    Encode::encode('utf8', $args->{password})
                ]);
    } elsif ($auth_type eq 'none') {
        $self->_send_packet(
                "b;str;str;str;",
                [
                    SSH2_MSG_USERAUTH_REQUEST(),
                    Encode::encode('utf8', $args->{user}),
                    "ssh-connection",
                    "none",
                ]);
    } else {
        return 0;
    }

    $self->_wait_for({SSH_SESSION_STATE_AUTHENTICATED() => 1});
    return $self->{session_state} == SSH_SESSION_STATE_AUTHENTICATED();
}

sub execute_command {
    my ($self, $command) = @_;
    my $free_channel = $self->_get_free_channel_number();
    Assert(defined $free_channel, "there is no free channel");
    $self->_channel_create("session", $free_channel)
        or return 0;

    $self->_send_packet(
            "b;u32;str;bool;str;",
            [
                SSH2_MSG_CHANNEL_REQUEST(),
                $self->{channel_state}[$free_channel][1],
                "exec",
                0,
                $command,
            ]);

    $self->_channel_close($free_channel)
        or return 0;
     
    return 1;
}

sub _channel_create {
    my ($self, $type, $number) = @_;
    $self->log("creating channel $number with type '$type'", SSH_LOG_PROTOCOL());
    $self->{channel_state}[$number][0] = CHANNEL_OPENING();
    $self->_send_packet(
            "b;str;u32;u32;u32;",
            [
                SSH2_MSG_CHANNEL_OPEN(),
                $type,
                $number,
                1024,
                1024,
            ]);
    $self->_wait_for({}, sub { 
            $self->{channel_state}[$number][0] == CHANNEL_OPENED()
                || $self->{channel_state}[$number][0] == CHANNEL_CLOSED() });
    return $self->{channel_state}[$number][0] == CHANNEL_OPENED();
}

sub _channel_close {
    my ($self, $number) = @_;
    $self->log("closing channel $number", SSH_LOG_PROTOCOL());
    $self->{channel_state}[$number][0] = CHANNEL_CLOSING();
    $self->_send_packet(
            "b;u32;",
            [
                SSH2_MSG_CHANNEL_CLOSE(),
                $self->{channel_state}[$number][1],
            ]);
    $self->_wait_for({}, sub { $self->{channel_state}[$number][0] == CHANNEL_CLOSED() });
    return $self->{channel_state}[$number][0] == CHANNEL_CLOSED();
}

sub _ask_service {
    my ($self, $service) = @_;
    $self->{auth_packet_state} = SERVICE_INIT();
    $self->_send_packet(
            "b;str;",
            [
                SSH2_MSG_SERVICE_REQUEST(),
                $service,
            ]);
    $self->_wait_for({}, sub { $self->{auth_packet_state} == SERVICE_ACCEPT(); });
    return $self->{auth_packet_state} == SERVICE_ACCEPT();
}

sub _wait_for {
    my ($self, $session_state, $additional) = @_;
    $additional ||= sub { 0 };
    while (! exists {
                %{$session_state},
                SSH_SESSION_STATE_ERROR() => 1,
                SSH_SESSION_STATE_DISCONNECTED() => 1,
            }->{$self->{session_state}} 
        && ! $additional->())
    {
        # reading response
        $self->{socket_obj}->process(1);
    }
}

sub _get_free_channel_number {
    my ($self) = @_;
    my $i = 0;
    for my $ch (@{$self->{channel_state}}) {
        if ($ch->[0] == CHANNEL_CLOSED()) {
            return $i;
        }
        $i++;
    }
    return;
}

sub _dh_init {
    my ($self) = @_;
    $self->{dh_g} = Math::BigInt->new('2');
    $self->{dh_p} = Math::BigInt->new(
        "0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74"
        . "020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
        . "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
        . "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF");
}

sub _handle_events {
    my ($self) = @_;
    $self->_wait_for({SSH_SESSION_STATE_AUTHENTICATING() => 1});
}

sub _handle_banner {
    my ($self) = @_;
    
    if ($self->{session_state} != SSH_SESSION_STATE_SOCKET_CONNECTED() 
            || !$self->{in_buffer} || $self->{in_buffer} !~ /^SSH[^\r\n]+\r\n$/) 
    {
        $self->{session_state} = SSH_SESSION_STATE_ERROR();
        return;
    }
    
    $self->{dh_V_S} = $self->{in_buffer};
    $self->{dh_V_S} =~ s/\r\n$//;

    $self->{session_state} = SSH_SESSION_STATE_BANNER_RECEIVED();
}

sub _handle_exception {
    my ($self, $message) = @_;
    $self->{session_state} = SSH_SESSION_STATE_ERROR();
    $self->log($message, SSH_LOG_RARE());
}

sub handle_connection {
    my ($self) = @_;

    my $callback = {
        SSH_SESSION_STATE_NONE() => sub {},
        SSH_SESSION_STATE_CONNECTING() => sub {},
        SSH_SESSION_STATE_SOCKET_CONNECTED() => sub {},
        SSH_SESSION_STATE_BANNER_RECEIVED() => sub { $self->_handle_banner_received(); },
        SSH_SESSION_STATE_INITIAL_KEX() => sub {},
        SSH_SESSION_STATE_KEXINIT_RECEIVED() => sub { $self->_handle_kexinit_received(); },
        SSH_SESSION_STATE_DH() => sub { $self->_handle_dh_state(); },
        SSH_SESSION_STATE_ERROR() => sub {},
    }->{$self->{session_state}};

    if ($callback) {
        $callback->();
    }
}

sub _handle_banner_received {
    my ($self) = @_;
    $self->log("banner received: " . $self->{in_buffer}, SSH_LOG_RARE());

    if ($self->{in_buffer} =~ /^SSH-(?:2|1\.99)/) {
        $self->{version} = 2;
        $self->{socket_obj}->data_callback(\&_handle_packet_callback);
    } elsif (substr($self->{in_buffer}, 4, 1) eq '1') {
        $self->{version} = 1;
        $self->{socket_obj}->data_callback(\&_handle_packet_callback1);
    } else {
        $self->{session_state} = SSH_SESSION_STATE_ERROR();
        return;
    }

    $self->{dh_V_C} = $self->{version} == 2 ? CLIENTBANNER2() : CLIENTBANNER1();

    $self->{session_state} = SSH_SESSION_STATE_INITIAL_KEX();
    $self->_send_banner();
}

sub _send_banner {
    my ($self) = @_;
    $self->{out_buffer} = sprintf("%s\r\n", 
                            $self->{version} == 2 ? CLIENTBANNER2() : CLIENTBANNER1());
    $self->{socket_obj}->process();
}

sub _send_packet {
    my ($self, $format, $elements) = @_;

    $self->log("sending packet: $elements->[0]", SSH_LOG_PROTOCOL());

    my $blocksize = $self->{cur_crypto}{in_cipher}
        ? List::Util::max(8, $self->{cur_crypto}{in_cipher}{blocksize})
        : 8;
    my $macsize = $self->{cur_crypto}{in_cipher} ? SSH::Cryptor::SHA_DIGEST_LEN : 0;

    my $payload = SSH::Packet::Build($format, $elements);

    if ($elements->[0] == SSH2_MSG_KEXINIT()) {
        $self->{dh_I_C} = $payload;
    }

    my $payload_length = length($payload);
    my $packet_length = $payload_length + 4 + 1;
    my $padding_length = ($blocksize - $packet_length % $blocksize);
    if ($padding_length < 4) {
        $padding_length += $blocksize;
    }
    $packet_length += $padding_length;

    # packet_length doesn't include size of himself
    $packet_length -= 4;

    $self->{out_buffer}
        = SSH::Packet::Build("u32;b;b$payload_length;b$padding_length;",
        [
            $packet_length,
            $padding_length,
            $payload,
            "x" x $padding_length,
        ]);

    if ($self->{cur_crypto}{in_cipher}) {
        $self->log("before encryption: " . DumpMemory($self->{out_buffer}));
        my $packet_hmac = $self->{cur_crypto}->hmac(
                pack("N", $self->{cur_crypto}{out_seq}) . $self->{out_buffer});
        $self->{out_buffer} = $self->{cur_crypto}->encrypt($self->{out_buffer});
        $self->log("packet_hmac: " . DumpMemory($packet_hmac));
        $self->{out_buffer} .= $packet_hmac;
    }
    $self->log("out seq: " . $self->{cur_crypto}{out_seq});
    $self->{cur_crypto}{out_seq}++;

    $self->log("sent packet with length(" . length($self->{out_buffer}) 
            . ")\ndump: " . DumpMemory($self->{out_buffer}));
    $self->{socket_obj}->process();
}

sub _handle_packet_callback {
    my ($self) = @_;

    my $blocksize = $self->{cur_crypto}{in_cipher}
        ? List::Util::max(8, $self->{cur_crypto}{in_cipher}{blocksize})
        : 8;
    my $macsize = $self->{cur_crypto}{in_cipher} ? SSH::Cryptor::SHA_DIGEST_LEN : 0;

    if ($self->{packet_state} == PACKET_STATE_INIT()) {
        if (length($self->{in_buffer}) < $blocksize) {
            $self->{socket_obj}->reset_buffer(0);
            return;
        }

        my $len = $self->_packet_decrypt_len($blocksize);
        $self->log("got packet len: $len", SSH_LOG_PROTOCOL());

        $self->{in_packet}{len} = $len;
        $self->{packet_state} = PACKET_STATE_SIZEREAD();
        if (length($self->{in_buffer}) + length($self->{temp_in_buffer}) >= $len + 4 + $macsize) {
            $self->{socket_obj}->reset_buffer(1); 
            # going further
            $self->_handle_packet_callback();
        }
    } elsif ($self->{packet_state} == PACKET_STATE_SIZEREAD()) {
        my $len = $self->{in_packet}{len};
        if (length($self->{in_buffer}) + length($self->{temp_in_buffer}) < $len + 4 + $macsize) {
            $self->{socket_obj}->reset_buffer(0); 
            return;
        } else {
            $self->{socket_obj}->reset_buffer(1); 
        }

        $self->log("read whole packet with length($len)", SSH_LOG_PROTOCOL());

        if ($self->{cur_crypto}{in_cipher}) {
            Assert($macsize > 0, "zero macsize");
            my $hmac = substr($self->{in_buffer}, -1 * $macsize);
            $self->{in_buffer} = $self->{temp_in_buffer} . $self->{cur_crypto}->decrypt(
                    substr($self->{in_buffer}, 0, $len + 4 - $blocksize));
            $self->log("decrypted packet: " . DumpMemory($self->{in_buffer}), SSH_LOG_PROTOCOL());
            unless ($self->{cur_crypto}->verify_hmac(
                    pack("N", $self->{cur_crypto}{in_seq}) . $self->{in_buffer}, $hmac)) 
            {
                $self->log("HMAC error: '$hmac'", SSH_LOG_RARE());
                $self->{session_state} = SSH_SESSION_STATE_ERROR();
                return;
            }
            $self->{in_buffer} .= $hmac;
            $self->{temp_in_buffer} = "";
        }
        $self->log("in seq: " . $self->{cur_crypto}{in_seq});
        $self->{cur_crypto}{in_seq}++;

        my $padding_length = ord(substr($self->{in_buffer}, 4, 1));
        if ($padding_length < 4) {
            $self->log("Invalid padding length: $padding_length", SSH_LOG_PROTOCOL());
            $self->{session_state} = SSH_SESSION_STATE_ERROR();
            return;
        }

        # TODO compression

        $self->{in_packet}{type} = ord(substr($self->{in_buffer}, 5, 1));
        $self->log(sprintf("packet type: %d", $self->{in_packet}{type}), SSH_LOG_PROTOCOL());
        $self->{in_packet}{content} = substr($self->{in_buffer}, 5, $len - 1 - $padding_length);

        $self->{packet_state} = PACKET_STATE_PROCESSING();
        $self->_process_packet_callback();
        $self->log(sprintf("processed packet %d", $self->{in_packet}{type}), SSH_LOG_PROTOCOL());

        $self->{packet_state} = PACKET_STATE_INIT();
        $self->log("in_buffer size: " . length($self->{in_buffer})
                    . ", processed packet: " . ($len + 4 + $macsize));
        $self->{in_buffer} = substr($self->{in_buffer}, $len + 4 + $macsize);
        if ($self->{in_buffer}) {
            $self->log("handling another packet without refilling");
            $self->_handle_packet_callback();
        }
    } elsif ($self->{packet_state} == PACKET_STATE_PROCESSING()) {
        # do we need this state?
    } else {
        $self->log(sprintf("Unknown type: %d", $self->{session_state}), SSH_LOG_PROTOCOL());
        $self->{session_state} = SSH_SESSION_STATE_ERROR();
        return;
    }
}

sub _packet_decrypt_len {
    my ($self, $blocksize) = @_;
    my $len = substr($self->{in_buffer}, 0, $blocksize);
    if ($self->{cur_crypto}{in_cipher}) {
#        $self->log("decryptIV: " . DumpMemory($self->{cur_crypto}{decryptIV}));
#        $self->log("decryptkey " . DumpMemory($self->{cur_crypto}{decryptkey}));
#        $self->log("encryptkey " . DumpMemory($self->{cur_crypto}{encryptkey}));
#        $len = substr($self->{cur_crypto}->decrypt($len), 0, 4);
        $len = $self->{cur_crypto}->decrypt($len);
        $self->{temp_in_buffer} = $len;
        $self->{in_buffer} = substr($self->{in_buffer}, $blocksize);
#        $self->log("crypted value: " . DumpMemory($inb));
#        $self->log("decrypted value: " . DumpMemory($y));
#        $self->log("decryptIV: " . DumpMemory($self->{cur_crypto}{decryptIV}));
#        $self->log("decryptkey " . DumpMemory($self->{cur_crypto}{decryptkey}));
#        $self->log("encryptkey " . DumpMemory($self->{cur_crypto}{encryptkey}));
    }
    return unpack("N", $len);
}

sub _process_packet_callback {
    my ($self) = @_;

    my $type = $self->{in_packet}{type};
    $self->log(sprintf("Dispatching packet with type: %d", $type), 
            SSH_LOG_PROTOCOL());
    exists DEFAULT_PACKET_HANDLERS()->{$type}
        or die "unimplemented type: $type";
    DEFAULT_PACKET_HANDLERS()->{$type}->($self);
}

sub _handle_dh_state {
    my ($self) = @_;
    if ($self->{dh_state} == DH_STATE_FINISHED()) {
        $self->{session_state} = SSH_SESSION_STATE_AUTHENTICATING();
        $self->log("going to authenticating state");
    }
}

sub dh_handshake {
    my ($self) = @_;

    $self->log("dh_handshake with dh_state: $self->{dh_state}");
    if ($self->{dh_state} == DH_STATE_INIT()) {
        $self->{dh_x} = Math::BigInt->new(
                    "0x" . join("", map {sprintf("%02X", rand(256))} (1..16))
                );

        $self->{dh_e} = $self->{dh_g}->copy();
        $self->{dh_e}->bmodpow($self->{dh_x}, $self->{dh_p});

        $self->_send_packet("b;mpi;", [
                    SSH2_MSG_KEXDH_INIT(),
                    $self->{dh_e},
                ]);

        $self->{dh_state} = DH_STATE_INIT_SENT();
    }
}

# callbacks follow ###################################################

sub _handle_kexinit {
    my ($self) = @_;
    $self->log("PACKET KEXINIT was received", SSH_LOG_PROTOCOL());

    $self->log("packet content: " . DumpMemory($self->{in_packet}{content}));

    $self->{dh_I_S} = $self->{in_packet}{content};
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
              "b;"
            . "b16;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "bool;"
            . "u32;");

    $it->next(); $it->next();

    $self->{kex_algorithm} = $self->_best_algorithm(SSH::Cryptor::KexAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{server_host_algorithm} = $self->_best_algorithm(SSH::Cryptor::HostKeyAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{encryption_cs} = $self->_best_algorithm(SSH::Cryptor::EncryptionAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{encryption_sc} = $self->_best_algorithm(SSH::Cryptor::EncryptionAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{mac_cs} = $self->_best_algorithm(SSH::Cryptor::MacAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{mac_sc} = $self->_best_algorithm(SSH::Cryptor::MacAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{compression_cs} = $self->_best_algorithm(SSH::Cryptor::CompressionAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{compression_sc} = $self->_best_algorithm(SSH::Cryptor::CompressionAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{language_cs} = $self->_best_algorithm(SSH::Cryptor::LanguageAlgorithms(), 
            [ split /,/, $it->next() ]);
    $self->{language_sc} = $self->_best_algorithm(SSH::Cryptor::LanguageAlgorithms(), 
            [ split /,/, $it->next() ]);

    my $kex_packet_follows = $it->next();
    $it->next();

    for my $algo ( qw(kex_algorithm
      server_host_algorithm
      encryption_cs
      encryption_sc
      mac_cs
      mac_sc
      compression_cs compression_sc) )
    {
        unless (defined $self->{$algo}) {
            $self->log("unsupported $algo algorithm");
            $self->{session_state} = SSH_SESSION_STATE_ERROR();
            return;
        }
    }

    $self->{session_state} = SSH_SESSION_STATE_KEXINIT_RECEIVED();

    $self->log("dumping algorithms: "
            . join(",\n", 
                map { "$_=$self->{$_}" } qw(kex_algorithm server_host_algorithm encryption_cs
                        encryption_sc mac_cs mac_sc compression_cs compression_sc)));
}

sub _handle_disconnect {
    my ($self) = @_;

    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
            "b;u32;str;str;");
    $it->next(); $it->next();
    my $reason = Encode::decode("utf8", $it->next());
    $it->next();

    $self->log("got disconnect packet with error '$reason', terminating", SSH_LOG_PROTOCOL());
    $self->{session_state} = SSH_SESSION_STATE_ERROR();
}

sub _handle_unimplemented {
    my ($self) = @_;
    $self->log("got unimplemented packet", SSH_LOG_PROTOCOL());
    $self->{session_state} = SSH_SESSION_STATE_ERROR();
}

sub _handle_service_accept {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
            "b;str;");
    $it->next();
    $self->log("accepted service " . $it->next());
    $self->{auth_packet_state} = SERVICE_ACCEPT();
}

sub _handle_kexinit_received {
    my ($self) = @_;

    #send kexinit packet
    $self->_send_packet(
              "b;"
            . "b16;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "nml;"
            . "bool;"
            . "u32;",
            [
                SSH2_MSG_KEXINIT(),
                "x" x 16,
                join(",", @{SSH::Cryptor::KexAlgorithms()}),
                join(",", @{SSH::Cryptor::HostKeyAlgorithms()}),
                join(",", @{SSH::Cryptor::EncryptionAlgorithms()}),
                join(",", @{SSH::Cryptor::EncryptionAlgorithms()}),
                join(",", @{SSH::Cryptor::MacAlgorithms()}),
                join(",", @{SSH::Cryptor::MacAlgorithms()}),
                join(",", @{SSH::Cryptor::CompressionAlgorithms()}),
                join(",", @{SSH::Cryptor::CompressionAlgorithms()}),
                join(",", @{SSH::Cryptor::LanguageAlgorithms()}),
                join(",", @{SSH::Cryptor::LanguageAlgorithms()}),
                0,
                0,
            ],
        );

    $self->{session_state} = SSH_SESSION_STATE_DH();
    $self->dh_handshake();
}

sub _handle_kex_dh_reply {
    my ($self) = @_;
    $self->log("got kex dh reply", SSH_LOG_PROTOCOL());

    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
              "b;"
            . "str;"
            . "mpi;"
            . "str;");

    $it->next();
    $self->{dh_K_S} = $it->next(); 
    $self->{dh_f} = $it->next();
    $self->{server_h_signature} = $it->next();

    $self->{dh_K} = $self->{dh_f}->copy();
    $self->{dh_K}->bmodpow($self->{dh_x}, $self->{dh_p});

    $self->{dh_state} = DH_STATE_NEWKEYS_SENT();
    $self->_send_packet("b;", [ SSH2_MSG_NEWKEYS() ]);
}

sub _handle_msg_newkeys {
    my ($self) = @_;
    if ($self->{dh_state} != DH_STATE_NEWKEYS_SENT()) {
        $self->log("wrong state in newkeys: $self->{dh_state}", SSH_LOG_RARE());
        $self->{session_state} = SSH_SESSION_STATE_ERROR();
        return;
    }

    $self->log("got newkeys packet", SSH_LOG_PROTOCOL());

    my $packet_dh_H = SSH::Packet::Build(
                "str;str;str;str;str;"
                . "mpi;mpi;mpi;",
                [
                    $self->{dh_V_C}, 
                    $self->{dh_V_S},
                    $self->{dh_I_C},
                    $self->{dh_I_S},
                    $self->{dh_K_S},
                    $self->{dh_e},
                    $self->{dh_f},
                    $self->{dh_K}
                ]);

    $self->log("pre H : " . DumpMemory($packet_dh_H, 1));

    $self->{dh_H} = Digest::SHA::sha1($packet_dh_H);

    $self->{session_id} ||= $self->{dh_H};

    $self->_set_crypto_algorithms();
    $self->_generate_session_keys();

    $self->{dh_state} = DH_STATE_FINISHED(); 
    $self->handle_connection();
}

sub _handle_userauth_success {
    my ($self) = @_;
    $self->{session_state} = SSH_SESSION_STATE_AUTHENTICATED();
}

sub _handle_userauth_failure {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, "b;nml;b;");
    $it->next();
    $self->log("got auth failure, here is a list of possible auths: " . ($it->next() || ""));
    $self->{session_state} = SSH_SESSION_STATE_ERROR();
}

sub _handle_channel_confirmation {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
            "b;u32;u32;u32;u32;");
    $it->next();
    my $recieve_channel = $it->next();
    my $sender_channel = $it->next();
    $it->next(); $it->next();
    $self->{channel_state}[$recieve_channel][0] = CHANNEL_OPENED();
    $self->{channel_state}[$recieve_channel][1] = $sender_channel;
}

sub _handle_channel_failure {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, 
            "b;u32;u32;str;str;");
    $it->next();
    my $recieve_channel = $it->next();
    $it->next();
    my $description = Encode::decode('utf8', $it->next());
    $it->next();
    $self->log("channel $recieve_channel was closed");
    $self->{channel_state}[$recieve_channel][0] = CHANNEL_CLOSED();
}

sub _handle_channel_eof {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, "b;u32;");
    $it->next();
    $self->{channel_state}[$it->next()][2] = 1;
}

sub _handle_channel_closed {
    my ($self) = @_;
    my $it = SSH::Packet::FieldIterator->new($self->{in_packet}{content}, "b;u32;");
    $it->next();
    my $recieve_channel = $it->next();
    $self->{channel_state}[$recieve_channel][0] = CHANNEL_CLOSED();
}

sub _set_crypto_algorithms {
    my ($self) = @_;

    $self->log("generating session algorithms ", SSH_LOG_PROTOCOL());

    my $in_seq = $self->{cur_crypto}{in_seq};
    my $out_seq = $self->{cur_crypto}{out_seq};

    #TODO init other algorithms
    my $crypto = $self->{cur_crypto} = SSH::Cryptor->new();
    $crypto->{in_seq} = $in_seq;
    $crypto->{out_seq} = $out_seq;
    Assert($crypto->init_in_cipher($self->{encryption_sc}) == 1);
    Assert($crypto->init_out_cipher($self->{encryption_cs}) == 1);
}

sub _generate_session_keys {
    my ($self) = @_;

    $self->log("generating session keys ", SSH_LOG_PROTOCOL());

    my $lh = length($self->{dh_H});
    my $ls = length($self->{session_id});
    my $format = "mpi;b$lh;b1;b$ls;";
    my $crypto = $self->{cur_crypto};
    $crypto->{encryptIV} = substr(Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'A', $self->{session_id}])
    ), 0, 8);
    $crypto->{decryptIV} = substr(Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'B', $self->{session_id}])
    ), 0, 8);

    my $encrypt_key = Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'C', $self->{session_id}])
    );
    my $decrypt_key = Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'D', $self->{session_id}])
    );
    if ($crypto->{in_cipher}{keysize} > SSH::Cryptor::SHA_DIGEST_LEN() * 8) {
        $decrypt_key = Digest::SHA::sha1(
                "mpi;b$lh;b$lh;", [ $self->{dh_K}, $self->{dh_H}, $decrypt_key ]);
    }
    if ($crypto->{out_cipher}{keysize} > SSH::Cryptor::SHA_DIGEST_LEN() * 8) {
        $encrypt_key = Digest::SHA::sha1(
                "mpi;b$lh;b$lh;", [ $self->{dh_K}, $self->{dh_H}, $encrypt_key ]);
    }
    $crypto->set_encryptkey($encrypt_key);
    $crypto->set_decryptkey($decrypt_key);

    $crypto->{encryptMAC} = Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'E', $self->{session_id}])
    );
    $crypto->{decryptMAC} = Digest::SHA::sha1(
        SSH::Packet::Build($format, [ $self->{dh_K}, $self->{dh_H}, 'F', $self->{session_id}])
    );
}

sub _best_algorithm {
    my ($self, $client, $server) = @_;
    for my $c (@$client) {
        if (grep {$_ eq $c} @$server) {
            return $c;
        }
    }
    return;
}

sub log {
    my ($self, $message, $verbosity) = @_;
    $verbosity ||= SSH_LOG_ALL();
    if ($self->{log_verbosity} >= $verbosity) {
        print STDERR $message . "\n";
    }
}

sub Assert {
    my ($x, $m) = @_;
    $x or die $m;
}

sub DumpMemory {
    my ($r, $ascii) = @_;
    return join "", map {
        ($ascii && ord($_) >= 32 && ord($_) <= 126)
            ? $_ 
            : sprintf("%02X", ord($_))
        } split //, $r;
}

1;

