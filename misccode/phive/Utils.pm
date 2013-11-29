package Utils;

use strict;
use warnings;

sub bloat {
    my ($filename) = @_;
    local $/;
    open my $fh, $filename
        or die "open filename $filename error: $!";
    <$fh>;
}

sub write_to_file {
    my ($filename, $contents) = @_;
    open my $filehandle, "| perltidy > $filename" #">$filename" 
            or die "can't open $filename for writing: $!";
    print {$filehandle} $$contents;
    close $filehandle or die "can't close $filename: $!";
}

sub strip {
    my $what = shift;
    $what =~ tr/'"//d;
    return $what;
}

sub split_id {
    my ($what) = @_;
    my @results;
    while ($what =~ /((?:[A-Z]|\$[A-Z])[^A-Z\$]*)/g) {
        push(@results, lc($1)); 
    }
    return @results;
}

sub combine_id {
    return join "", map {up_first_letter(lc($_))}  @_;
}

sub up_first_letter {
    my ($x) = shift;
    $x =~ s/^\$([a-z])/'$' . uc($1)/e
        or return ucfirst($x);
    return $x;
}

1;
