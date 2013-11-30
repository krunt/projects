package InsertClauseParser;

use strict;
use warnings;
use InsertClause;
use PConstants;
use Asserter;
use Digest::MD5 qw(md5_hex);

sub new { return bless {}, shift; }

sub parse {
    my ($self, $raw_statement, $pcontext) = @_;

    my $next_one = $raw_statement;
    if ($next_one->getName() == PConstants::TOK_DIR) {
        #it is a temporary table
        Asserter::assert_equals($next_one->getChild(0)->getName(), 
            PConstants::TOK_TMP_FILE, 
            "it is not TOK_TMP_FILE in InsertClauseParser");
     
        return InsertClause->new(_Generate_tempname(), 1, $pcontext);

    } elsif ($next_one->getName() == PConstants::TOK_TAB) {
        #it is ordinary table
        return InsertClause->new($next_one->getChild(0)->getText(), 
                                    0, $pcontext);
    }
    
    Asserter::assert(0, "Unknown destination statement");
}

sub _Generate_tempname {
    return md5_hex(rand() . rand() . rand());
}

1;
