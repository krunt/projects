package SSH::Packet;

use strict;
use warnings;

use constant TYPE_BYTE     => "b";
use constant TYPE_BOOLEAN  => "bool";
use constant TYPE_UINT32   => "u32";
use constant TYPE_UINT64   => "u64";
use constant TYPE_STRING   => "str";
use constant TYPE_MPINT    => "mpi";
use constant TYPE_NAMELIST => "nml";

use constant HANDLERS => {
    SSH::Packet::TYPE_BYTE() => sub {
        my ($el, $len) = @_;
        unless (defined $len) {
            return chr($el);
        } 
        return $el;
    },
    SSH::Packet::TYPE_BOOLEAN() => sub {
        return chr($_[0]);
    },
    SSH::Packet::TYPE_UINT32() => sub {
        my ($el) = @_;
        return pack("N", $el);
    },
    #TODO don't know what to do with it
    SSH::Packet::TYPE_UINT64() => sub {
        my ($el) = @_;
        return $el;
    },
    SSH::Packet::TYPE_STRING() => sub {
        my ($el) = @_;
        return pack("N", length($el)) . $el;
    },
    SSH::Packet::TYPE_MPINT() => sub {
        my ($el) = @_;
        my $hex = substr($el->as_hex(), 2);
        (length($hex) % 2) && ($hex = "0" . $hex);
        (hex("0x" . substr($hex, 0, 2)) & 0x80)
            && ($hex = "00" . $hex);
        my $binary = pack("H*", $hex);
        return pack("N", length($binary)) . $binary;
    },
    SSH::Packet::TYPE_NAMELIST() => sub {
        my ($el) = @_;
        return pack("N", length($el)) . $el;
    },
};

sub Build {
    my ($format, $mas) = @_;
    my $result = '';
    for my $el (@$mas) {
        $format =~ /^([^;]+);/
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
            die "there is no such type $type";
        }

        $result .= $callback->($el, $len);

        $format = substr($format, $orig_len);
    }
    return $result;
}

1;

