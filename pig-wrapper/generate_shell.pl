#!/usr/bin/perl 

use strict;
use warnings;

use XML::Simple ();
use Digest::MD5 ();
use POSIX ();
use MIME::Base64 ();

use constant { LOCAL => 0, GLOBAL => 1, };
use constant { MEMLIMIT => 3000 }; # M

my $jobident = shift or die "no jobident specified";
my ($pig_script_path) = grep {/\.pig$/} @ARGV 
    or die "pig-script is not specified";

my $pighome = "/home/aaa/stuff/hadoophacks/pig-0.12.0";
my $hadoophome = "/home/aaa/stuff/hadoophacks/hadoop12";
my $jobconfdir = $pighome . "/mine/jobhistory/$jobident";
my $dstdir = $hadoophome . "/mine/jobs";
my $srcdatapath = $hadoophome . "/data/";
my $srcdictspath = $hadoophome . "/dicts/";
my $mrwrapper_path = $pighome . "/mine/wrapper.pl";
my $wrapper_classpath = "$hadoophome/mywrappers.jar";
my $log4jpath = $pighome . "/mine/log4j.properties";
my $classpath = get_classpath();
my @filedicts = get_filedicts($pig_script_path);
my $tmp_prefix = "";
my $current_mode = LOCAL();

sub get_classpath {
    my $result;

    #$result = `$hadoophome/bin/hadoop classpath`;
    #chomp $result;
    #$result .= ":";

    $result .= $hadoophome . "/hadoop-core-1.2.2.jar:";
    $result .= $hadoophome . "/hadoop-client-1.2.2.jar:";
    $result .= $hadoophome . "/hadoop-tools-1.2.2.jar:";

    $result .= ":";
    $result .= $pighome . "/pig-0.12.0.jar:";
    $result .= $pighome . "/datafu.jar:";
    $result .= $pighome . "/mine/piggybank.jar:";
    $result .= $pighome . "/mine/stxeval.jar:";

    $result .= $wrapper_classpath;

    return $result;
}

sub get_filedicts {
    my $script_path = shift;
    my @pig_contents = do { open my $fd, $script_path or die $!; <$fd>; };
    my @dicts;
    for (my $i = 0; $i < @pig_contents; ++$i) {
        my $line = $pig_contents[$i];
        if ($line =~ /^--\s*dicts:\s*$/) {
            while (++$i < @pig_contents) {
                $pig_contents[$i] =~ /^--\s*([\w\.-]+)$/
                    or last;
                push(@dicts, $1);
            }
        }
    }
    return @dicts;
}

sub default_cmdline {
    my ($dir, $step, $cp, $files) = @_;
    return Cmdline->new({
            java_path => "java",
            classpath => $cp . ":" .
                ($current_mode == LOCAL()
                ? $pighome . "/mine/stxjava_precise.jar"
                : $pighome . "/mine/stxjava_lucid.jar"),
            mainclass => "org.PigWrapper",
            defines => { 'java.library.path' 
                => '/usr/local/share/java/lib-dynload', 
                'stream.mode' => 1, 
                'log4j.configuration' => 
                    ($current_mode == LOCAL() 
                    ? "file:$log4jpath" : "file:./log4j.properties"),
                'java.io.tmpdir' => '.',
                'pig.cachedbag.memusage' => 0.2, # optional
            },
            java_args => [ '-Xmx' . MEMLIMIT() . 'M', ],
            args => [ $dir, $current_mode == LOCAL() ? "local" : "global", $step],
            files => $files || [],
            filedicts => \@filedicts,
        });
}

sub get_requested_parallelism {
    my $dir = shift;
    my $result = `hadoop_reducer_number.sh $dir`;
    $result += 0;
    return $result == -1 ? undef : $result;
}

sub get_loads {
    my $dir = shift;
    return map {chomp; /^\s+$/ && return (); $_; } `hadoop_load_viewer.sh $dir`;
}

sub get_stores {
    my ($dir, $is_map) = @_;
    my $map_or_reduce = $is_map ? "map" : "reduce";
    my @stores = map {chomp; /^\s+$/ && return (); $_; } 
        `hadoop_store_viewer.sh $dir $map_or_reduce`;
    return @stores;
}

sub istmpstore { return $_[0] =~ /\btmp\b/ ? 1 : 0; }

sub get_conf {
    my $confpath = shift;
    -f $confpath or die "$confpath does not exist";
    my %result;
    my $conf = XML::Simple::XMLin($confpath);
    for my $k (keys %{$conf->{property}}) {
        $result{$k} = $conf->{property}{$k}{value};
    }
    do_default_substitutions(\%result);
    return \%result;
}

sub do_default_substitutions {
    my $conf = shift;
    $conf->{"hadoop.tmp.dir"} =~ s/\$\{user.name\}/alexey/;
}

sub path2local {
    my ($conf, $path) = @_;

    my $workdir = $conf->{"mapred.working.dir"};
    my $fsdefault = $conf->{"fs.default.name"};
    my $tmpdir = $conf->{"hadoop.tmp.dir"};

    $path =~ s/^$workdir\///;
    $path =~ s/^$fsdefault//;
    $path =~ s/^$tmpdir\///;

    if (istmpstore($path)) {
        $path =~ s/^.*\/([^\/]+)$/$1/;
    }

    return $path;
}

sub tmppath { return $tmp_prefix .  Digest::MD5::md5_hex(rand()); }

sub generate_task {
    my $task = Task->new();
    my @jobs = do { open my $fd, $jobconfdir . "/joblist" or die $!; 
        map {chomp; s///; $_; } <$fd>; };
    
    for  my $job (@jobs) {
        my $jobdir = "$jobconfdir/$job";
        my $confpath = "$jobdir/job.xml"; 
    
        my $conf = get_conf($confpath);
    
        my $has_combine = $conf->{"mapreduce.combine.class"};
        my $has_reduce = $has_combine || $conf->{"mapreduce.reduce.class"};
    
        my $map = Cmd->new({type => "map", "cmdline" 
            => default_cmdline($jobdir, "map", "$jobdir/job.jar", 
                ["$jobdir/job.xml", $log4jpath, ])});
        $task->add_step($map);
    
        my $loads = 3;
        for (sort &get_loads($jobdir)) {
            $map->add_input(path2local($conf, $_), $loads, istmpstore($_));
            $loads += 2;
        }
    
        my @stores = sort &get_stores($jobdir, 1);
        my $stores = 4;
        for (@stores) {
            $map->add_output(path2local($conf, $_), $stores, istmpstore($_));
            $stores += 2;
        }
    
        # for now just ignore combine step
        if ($has_reduce) {
            my $reduce = Cmd->new({type => "reduce", 
                "cmdline" => default_cmdline($jobdir, "reduce", 
                "$jobdir/job.jar", ["$jobdir/job.xml", $log4jpath, ]), 
                parallelism => &get_requested_parallelism($jobdir), });
    
            # adding default path between map and reduce
            my $tmppath = tmppath();
            $map->add_output($tmppath, 1, 1);
            $reduce->add_input($tmppath, 0, 1);
    
            # no loads for reduce
    
            my @stores = sort &get_stores($jobdir, 0);
            my $stores = 4;
            for (@stores) {
                $reduce->add_output(path2local($conf, $_), 
                    $stores, istmpstore($_));
                $stores += 2;
            }
    
            $task->add_step($reduce);
        }
    }
    return $task;
}

my $i = 0;
my @modes = (LOCAL(), GLOBAL());
for my $compiler (ShellCompiler->new(), MRCompiler->new()) 
{
    $current_mode = $modes[$i++];
    my $task = generate_task();
    my $output = $compiler->compile($task);
    $compiler->store($output);
}


1;

package Cmdline; 

sub new {
    my ($cls, $args) = @_;
    return bless {%$args}, $cls;
}

sub _quote {
    shift;
    my $noquote_class = '.\\w/\\-@,:';
    my $quoted = join '',
        map { ( m|\A'\z|                  ? "\\'"    :
                m|\A'|                    ? "\"$_\"" :
                m|\A[$noquote_class]+\z|o ? $_       :
                                          "'$_'"   )
          } split /('+)/, $_[0];
    length $quoted ? $quoted : "''";
}

sub get_javapath  { return $_[0]->{java_path}; }
sub get_mainclass { return $_[0]->{mainclass}; }
sub get_classpath { return $_[0]->{classpath}; }
sub get_defines   { return $_[0]->{defines}; }
sub get_java_arguments { return $_[0]->{java_args}; }
sub get_arguments { return $_[0]->{args}; }
sub get_files     { return $_[0]->{files}; }
sub get_filedicts { return $_[0]->{filedicts}; }

1;

package Cmd;

sub new {
    my ($cls, $args) = @_;
    return bless {%$args}, $cls;
}

sub type { $_[0]->{type} }

# path, order_index, is_tmp
sub add_input {
    $_[0]->{inputs} ||= [];
    push(@{$_[0]->{inputs}}, [$_[1], $_[2], $_[3]]);
}

# path, order_index, is_tmp
sub add_output {
    $_[0]->{outputs} ||= [];
    push(@{$_[0]->{outputs}}, [$_[1], $_[2], $_[3]]);
}

sub get_inputs { return $_[0]->{inputs} || []; }
sub get_outputs { return $_[0]->{outputs} || []; }
sub get_cmdline { return $_[0]->{cmdline}; }
sub get_parallelism { return $_[0]->{parallelism}; }

sub get_tmp_input_map {
    my $self = shift;
    my $inpmap = '';
    for my $i (0..$#{$self->{inputs}}) {
        if ($self->{inputs}[$i][2]) {
            vec($inpmap, $i, 1) = 1;
        }
    }
    return MIME::Base64::encode_base64($inpmap, "");
}

sub get_stdin {
    for (@{$_[0]->{inputs}}) {
        if (!$_->[1]) { return $_; }
    }
    return;
}

sub get_stdout {
    for (@{$_[0]->{outputs}}) {
        if ($_->[1] == 1) { return $_; }
    }
    return;
}

sub get_tmp_stores {
    my @stores;
    for (@{$_[0]->{outputs}}) {
        if ($_->[2]) { push(@stores, $_); }
    }
    return @stores;
}

1;


package Task;

sub new {
    my ($cls, $args) = @_;
    return bless {%{$args || {}}}, $cls;
}

sub add_step {
    my ($self, $step) = @_;
    push(@{$self->{steps}}, $step);
}

sub get_steps {
    my $self = shift;
    return $self->{steps} || [];
}

1;


package ShellCompiler;

sub new {
    my ($cls, $args) = @_;
    return bless {%{$args || {}}}, $cls;
}

sub _convert_step_to_string {
    my ($self, $step) = @_;

    my $defines = $step->get_cmdline()->get_defines();
    my $cmdline = sprintf("%s %s -cp \$COMMON_CLASSPATH:%s %s %s %s",
        $step->get_cmdline()->get_javapath(), 
        join(" ", @{$step->get_cmdline()->get_java_arguments() || []}),
        $step->get_cmdline()->get_classpath(),
        join(" ", map {" -D$_=" . $defines->{$_}} keys %$defines),
        $step->get_cmdline()->get_mainclass(), 
        join(" ", @{$step->get_cmdline()->get_arguments()}));
        
    return $cmdline;
}

sub _input_with_prefix {
    my ($self, $path) = @_;
    return !$path->[2] ? $srcdatapath . $path->[0] : $path->[0];
}

sub compile {
    my ($self, $task) = @_;

    my $result = "#!/bin/bash\nset -e\nCOMMON_CLASSPATH=\"$classpath\"\n\n";
    my $steps = $task->get_steps();
    for my $i (0 .. @$steps - 1) {
        my $step = $steps->[$i];

        $result .= $self->_convert_step_to_string($step) . " ";

        for my $input (@{$step->get_inputs()}) {
            $result .= $input->[1] . "<" 
                . $self->_input_with_prefix($input) . " ";
        }

        for my $output (@{$step->get_outputs()}) {
            $result .= $output->[1] . ">" . $output->[0] . " ";
        }

        # filedicts
        for my $filedict (@{$step->get_cmdline()->get_filedicts()}) {
            system("ln -fs $srcdictspath/$filedict $jobconfdir/$filedict") 
                && die $!;
        }

        $result .= "\n";

        if ($step->type() eq 'map' && $i + 1 < @$steps) {
            $steps->[$i + 1]->type() eq 'reduce' 
                or next;

            $result .= sprintf("sort -k1,2 -t '\t' 0<%s 1>_x\n", 
                    $step->get_stdout()->[0]);
            $result .= sprintf("mv _x %s\n", 
                    $step->get_stdout()->[0]);
        }
    }

    # adding cleaning commands
    for my $step (@$steps) {
        for my $store ($step->get_tmp_stores()) {
            $result .= "rm -f " . $store->[0] . "\n";
        }
    }

    return $result;
}

sub store {
    my ($self, $output) = @_;
    open my $fd, ">$jobconfdir/start.sh" or die $!;
    print {$fd} $output;
    close($fd);
    chmod 0755, "$jobconfdir/start.sh";
}

1;

