use 5.014002;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
    NAME              => 'tests',
    VERSION_FROM      => 'tests.pm', # finds $VERSION
    INC               => "-I.. -I.",

    CCFLAGS => '-fPIC -x c++ -Werror -Waddress -Wno-write-strings -Wchar-subscripts -Wcomment -Wformat -Wmissing-braces -Wparentheses -Wreorder -Wreturn-type -Wsequence-point -Wsign-compare -Wstrict-aliasing -Wstrict-overflow=1 -Wswitch -Wtrigraphs -Wno-uninitialized -Wunknown-pragmas -Wno-unused-function -Wunused-label -Wunused-value -Wno-unused-variable -Wvolatile-register-var -Wno-variadic-macros',

    LIBS => '-lperl -lpcrecpp',
    LDDLFLAGS => '-shared',
    OPTIMIZE => '-O3',
    CC => 'g++',
    LD => 'g++',

    XSOPT => '-C++ -hiertype',
    XSPROTOARG => '-prototypes',
);
