package SSH::Packet::FieldIterator;

use strict;
use warnings;
use SSH::Packet ();
use Math::BigInt ();
use bytes;

use constant HANDLERS => {
    SSH::Packet::TYPE_BYTE() => sub {
        my ($self, $len) = @_;
        my $r = substr($self->{packet}, 0, $len);
        return [$r, $len];
    },
    SSH::Packet::TYPE_BOOLEAN() => sub {
        my ($self) = @_;
        return [ord(substr($self->{packet}, 0, 1)), 1];
    },
    SSH::Packet::TYPE_UINT32() => sub {
        my ($self) = @_;
        return [unpack("N", substr($self->{packet}, 0, 4)), 4];
    },
    SSH::Packet::TYPE_UINT64() => sub {
        my ($self) = @_;
        return [substr($self->{packet}, 0, 8), 8];
    },
    SSH::Packet::TYPE_STRING() => sub {
        my ($self) = @_;
        my $len = unpack("N", substr($self->{packet}, 0, 4));
        return [substr($self->{packet}, 4, $len), 4 + $len];
    },
    SSH::Packet::TYPE_MPINT() => sub {
        my ($self) = @_;
        my $len = unpack("N", substr($self->{packet}, 0, 4));
        return [Math::BigInt->new('0x' . unpack("H*", substr($self->{packet}, 4, $len))), 4 + $len];
    },
    SSH::Packet::TYPE_NAMELIST() => sub {
        my ($self) = @_;
        my $len = unpack("N", substr($self->{packet}, 0, 4));
        return [substr($self->{packet}, 4, $len), 4 + $len];
    },
};
    
sub new {
    my ($class, $packet, $format) = @_;
    return bless {
        packet => $packet,
        format => $format,
    }, $class;
}

sub next {
    my ($self) = @_;

    unless ($self->{packet}) {
        return undef;
    }

    $self->{format} =~ /^([^;]+);/
        or die "invalid format";

    my $orig_len = length($1) + 1;
    my $type = $1;
    my $len;
    if ($type =~ /^([^\d]+)(\d+)$/ && $1 ne 'u') {
        $type = $1; 
        $len = $2;
    }

    my $callback = HANDLERS()->{$type};
    unless ($callback) {
        die "invalid state: $type";
    }
    my $s = $callback->($self, $len || 1);
    $self->{packet} = ($s->[1] >= length($self->{packet})) ? "" : substr($self->{packet}, $s->[1]);
    $self->{format} = substr($self->{format}, $orig_len);
    return $s->[0];
}

1;

