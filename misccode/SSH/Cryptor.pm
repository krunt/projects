package SSH::Cryptor;

use strict;
use warnings;

use blib "MCrypto";
use MCrypto ();

use constant SHA_DIGEST_LEN => 20;
use constant EVP_MAX_MD_SIZE => 36;

sub new {
    my ($class) = @_;
    return bless {
        encryptIV => "\0" x (SHA_DIGEST_LEN() * 2),
        decryptIV => "\0" x (SHA_DIGEST_LEN() * 2),

        encryptkey => "\0" x (SHA_DIGEST_LEN() * 2),
        decryptkey => "\0" x (SHA_DIGEST_LEN() * 2),

        encryptMAC => "\0" x SHA_DIGEST_LEN(),
        decryptMAC => "\0" x SHA_DIGEST_LEN(),

        hmacbuf => "\0" x EVP_MAX_MD_SIZE(),

        in_cipher => '',
        out_cipher => '',

        in_seq  => 0,
        out_seq => 0,
    }, $class;
}

sub init_in_cipher {
    my ($self, $name) = @_;
    my @mas = List();
    for my $x (@mas) {
        if ($x->{name} eq $name) {
            $self->{in_cipher} = $x;
            return 1;
        }
    }
    return 0;
}

sub init_out_cipher {
    my ($self, $name) = @_;
    my @mas = List();
    for my $x (@mas) {
        if ($x->{name} eq $name) {
            $self->{out_cipher} = $x;
            return 1;
        }
    }
    return 0;
}

sub set_encryptkey {
    my ($self, $encryptkey) = @_;
    $self->{encryptkey} = substr($encryptkey, 0, $self->{in_cipher}{keysize} / 8);
}

sub set_decryptkey {
    my ($self, $decryptkey) = @_;
    $self->{decryptkey} = substr($decryptkey, 0, $self->{out_cipher}{keysize} / 8);
}

sub encrypt {
    my ($self, $buf) = @_;
    return MCrypto::packet_encrypt(
            $buf,
            $self->{encryptkey},
            \$self->{encryptIV},
            $self->{out_cipher}{index}
    );
}

sub decrypt {
    my ($self, $buf) = @_;
    return MCrypto::packet_decrypt(
            $buf,
            $self->{decryptkey},
            \$self->{decryptIV},
            $self->{in_cipher}{index});
}

sub hmac {
    my ($self, $buf) = @_;
    return MCrypto::compute_hmac(
            $buf,
            $self->{encryptMAC});
}

sub verify_hmac {
    my ($self, $buf, $mac) = @_;
    return MCrypto::packet_hmac_verify(
            $buf,
            $self->{decryptMAC},
            $mac);
}

sub KexAlgorithms {
    return [ qw(diffie-hellman-group1-sha1) ];
}

sub HostKeyAlgorithms {
    return [ qw(ssh-dss) ];
}

sub EncryptionAlgorithms {
    return [ map {$_->{name}} sort {$b->{name} cmp $a->{name}} List() ];
}

sub MacAlgorithms {
    return [ qw(hmac-sha1) ];
}

sub CompressionAlgorithms {
    return [ qw(none) ];
}

sub LanguageAlgorithms {
    return [];
}

sub List {
    my $mas = MCrypto::list_cryptors();
    my @result;
    my $i = 0;
    while (my @m = splice(@$mas, 0, 4)) {
        push(@result, {
                    name => $m[0], 
                    blocksize => $m[1],
                    keylen => $m[2],
                    keysize => $m[3],
                    "index" => $i++,
                });
    }
    return @result;
}

1;

