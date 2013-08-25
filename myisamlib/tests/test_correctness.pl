#!/usr/bin/perl

use DBI;
use Statbox::Utils ();
use Statbox::TSKVUtils ();
$opt_help=$opt_version=$opt_verbose=$opt_force=0;
$opt_user=$opt_database=$opt_password=undef;
$opt_host="localhost";
$opt_socket="/stuff/experiments/m1/build/mysqld.sock";
$opt_engine="MYISAM";
$opt_port=40001;
$exit_status=0;

$opt_user="root";
$opt_database="test";


$connect_opt="";
if ($opt_port)
{
  $connect_opt.= ";port=$opt_port";
}
if (length($opt_socket))
{
  $connect_opt.=";mysql_socket=$opt_socket";
}

$dbh = DBI->connect("DBI:mysql:$opt_database:${opt_host}$connect_opt",
		    $opt_user,
		    $opt_password,
		    { PrintError => 0})
  || die "Can't connect to database $opt_database: $DBI::errstr\n";

#$sth=$dbh->prepare(          #a b c d e f g                h 
#    "INSERT INTO one1 values (?,?,?,?,?,?,FROM_UNIXTIME(?),FROM_UNIXTIME(?),"
#    #i                j                k l m n o p q r s 
#  . "FROM_UNIXTIME(?),FROM_UNIXTIME(?),?,?,?,?,?,?,?,?,?,'x',0)");

#$sth=$dbh->prepare("INSERT INTO one2 values (?,?,?)");

my @fieldtypes = ('tinyint', 'smallint', 'mediumint', 'int', 'bigint', 'float', 'double', 
    'timestamp', 'date', 'time', 'datetime', 'varchar(<=1024)',
    'tinyblob', 'mediumblob', 'blob', 'longblob', 'char(<=1024)');

my @generators = (
qr/^tinyint/ => \&tinyint,
qr/^smallint/ => \&smallint,
qr/^int$/ => \&standardint,
qr/^float/ => \&genfloat,
qr/^double/ => \&gendouble,
qr/^bigint/ => \&bigint,
qr/^timestamp/ => \&timestamp,
qr/^date/ => \&gendate,
qr/^time/ => \&gentime,
qr/^datetime/ => \&gendate,
qr/^varchar\((\d+)\)/ => \&varchar,
#qr/^bit\((\d+)\)/ => \&genbit,
qr/^tinyblob/ => \&tinyblob,
qr/^mediumblob/ => \&mediumblob,
qr/^blob/ => \&blob,
qr/^longblob/ => \&longblob,
qr/^char\((\d+)\)/ => \&char,
);

sub generate_schema {
    my $field_count = int(rand(10)) + 1;
    my @fields;
    for my $c (1..$field_count) {
        my $fieldtype = $fieldtypes[int(rand(@fieldtypes))];
        $fieldtype =~ s/<=(\d+)/int(rand($1))+1/ge;
        push(@fields, ["m$c", $fieldtype]);
    }
    return \@fields;
}

for my $test (1..1000) {
    $dbh->do("drop table one5");

    my $fields = generate_schema();

    my $create_definition = sprintf("create table one5 (%s)",
            join(",", map {$_->[0] . " " . $_->[1]} @$fields));
    print STDERR $create_definition."\n";
    $dbh->do($create_definition);

    $sth= $dbh->prepare(sprintf("insert into one5 values(%s)",
            join(",", map {$_->[1]=~/date/?"FROM_UNIXTIME(?)":"?"} @$fields)));

    my $toadd = int(rand(1000)) + 1600;
    my @vars = ("") x @$fields;
    for my $c (1..$toadd) {
        for my $j (0..$#$fields) {
            my $field = $fields->[$j][1];
            for (my $i = 0; $i < @generators; $i += 2) {
                if ($field =~ $generators[$i]) {
                    $vars[$j] = $generators[$i+1]->($1||0);
                    last;
                }
            }
        }
        $sth->execute(@vars);
    }

    ($sth=$dbh->prepare("select * from one5"))->execute();

    my %hash;
    while ((my $row=$sth->fetchrow_hashref("NAME_lc"))) {
        $hash{Statbox::Utils::PackHashToString($row)}++;
    }
    if (scalar keys %hash == 0) {
        next;
    }

    my $command = sprintf("./test1 %s", join " ", map {$_->[0]} @$fields);
    my @lines = `$command`;
    for my $line (@lines) {
        chomp($line);
        $line =~ s/\\x(..)/chr(hex("0x$1"))/ge;
        $line =~ s/(^\w+|\t\w+)=NULL/$1/g;
        my %row;
        Statbox::TSKVUtils::decode("tskv\t$line", \%row);
        unless (%row) { die "can't parse string"; }
        if (! exists $hash{Statbox::Utils::PackHashToString(\%row)}) {
            use Data::Dumper;
            print Dumper(\%hash, Statbox::Utils::PackHashToString(\%row));
            die "not in hash";
        }
        if (!--$hash{Statbox::Utils::PackHashToString(\%row)}) {
            delete $hash{Statbox::Utils::PackHashToString(\%row)};
        }
    }

    if (%hash) {
        die "test is not done";
    } else {
        print "TEST OK\n";
    }

    $dbh->do("drop table one5");
}


sub tinyint { my $r = int(rand(2**8)); $r > (2**8)-1 ? $r - 2**8 : $r }
sub smallint { my $r = int(rand(2**16)); $r > (2**16)-1 ? $r - 2**16 : $r }
sub mediumint { my $r = int(rand(2**24)); $r > (2**24)-1 ? $r - 2**24 : $r }
sub standardint { my $r = int(rand(2**32)); $r > (2**32)-1 ? $r - 2**32 : $r }
sub bigint { my $r = int(rand(2**64)); $r > (2**64)-1 ? $r - 2**64 : $r }

sub genfloat { rand() * 10000 * (rand() < 0.5 ? -1 : 1) }
sub gendouble { rand() * 10000 * (rand() < 0.5 ? -1 : 1) }

sub genbit { join "", map {(rand() < 0.5 ? 0 : 1)} (1..$_[0]) }
sub timestamp { int(rand(2**32)) }
sub gendate { int(rand(2**32)) }
sub gentime { int(rand(2**32)) }
sub varchar { join "", map { ('a'..'z')[int(rand(26))] } (1..($_[0]<16?16:$_[0])) }
sub tinyblob { join "", map { ('a'..'z')[int(rand(26))] } (1..128) }
sub mediumblob { join "", map { ('a'..'z')[int(rand(26))] } (1..128) }
sub blob { join "", map { ('a'..'z')[int(rand(26))] } (1..128) }
sub longblob { join "", map { ('a'..'z')[int(rand(26))] } (1..128) }
sub char { join "", map { ('a'..'z')[int(rand(26))] } (1..$_[0]) }
