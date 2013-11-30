package Functions;

use strict;
use warnings;
use Function;
use AggregateFunction;

sub new {
    my $class = shift;
    return bless {
        #Aggregate functions
		COUNT0 => AggregateFunction->new(
						"COUNT",
						"my \$hits = 0;\n",
						"\$total{\$new_key}{hits} += \$hits;\n\$hits = 0;\n",
						"\$hits ++;\n",
						"\$hash{\$key}{hits} += \$hits;\n",
						"hits",
				),
		COUNT1 => AggregateFunction->new(
						"COUNT DISTINCT",
						"",
						"\$total{\$new_key}{uniq} ++;\n",
						"\n",
						"\$hash{\$key}{uniq} += \$uniq;\n",
						"uniq",
				),
		MIN0 => AggregateFunction->new(
						"MIN",
						"my \$min_element = (1<<30);\n",
						"\$total{\$new_key}{min} = \$min_element; \$min_element = (1<<30);\n",
						"\$min_element = min(\$val, \$min_element);\n",
						"\$hash{\$key}{min} ||= (1<<30); \$hash{\$key}{min} = min(\$min, \$hash{\$key}{min});\n",
						"min",
				),
		MAX0 => AggregateFunction->new(
						"MAX",
						"my \$max_element = (- (1<<31));\n",
						"\$total{\$new_key}{max} = \$max_element; \$max_element = (- (1<<31));\n",
						"\$max_element = max(\$val, \$max_element);\n",
						"\$hash{\$key}{max} ||= (- (1<<31)); \$hash{\$key}{max} = max(\$max, \$hash{\$key}{max});\n",
						"max",
				),
		SUM0 => AggregateFunction->new(
						"SUM",
						"my \$total_sum = 0;\n",
						"\$total{\$new_key}{sum} += \$total_sum;\n\$total_sum = 0;\n",
						"\$total_sum += \$val;\n",
						"\$hash{\$key}{sum} += \$sum;\n",
						"sum",
				),
		CONCAT0 => AggregateFunction->new(
						"CONCAT",
						"my \$concatted_value = '';",
						"\$total{\$new_key}{concat} = \$concatted_value;\n\$concatted_value = '';\n",
						"\$concatted_value .= \$val . ', ';\n",
						"\$hash{\$key}{concat} .= \$concat;\n",
						"concat",
				),
		XOR0 => AggregateFunction->new(
						"XOR",
						"my \$xorred_value = 0;\n",
						"\$total{\$new_key}{'xor'} = \$xorred_value;\n\$xorred_value = 0;\n",
						"\$xorred_value ^= \$val;\n",
						"\$hash{\$key}{'xor'} ^= \$xor;\n",
						"xor",
				),

        #Ordinary functions
        COUNT => Function->new("COUNT", 1, 1, '', 'hits'),
        COUNT_DI => Function->new("COUNT_DI", 1, 1, '', 'uniq'),
        SUM => Function->new("SUM", 1, 1, '', 'sum'),
        MIN => Function->new("MIN", 1, 1, '', 'min'),
        MAX => Function->new("MAX", 1, 1, '', 'max'),
        XOR => Function->new("XOR", 1, 1, '', 'xor'),
        CONCAT => Function->new("CONCAT", 1, 1, '', 'concat'),

        LENGTH => Function->new("LENGTH", 0, 1, 'length(__0__)'),
        REVERSE => Function->new("REVERSE", 0, 1, 'reverse(__0__)'),
        SUBSTR => Function->new("SUBSTR", 0, 3, 'substr(__0__, __1__, __2__)'),
        UPPER => Function->new("UPPER", 0, 1, 'uc(__0__)'),
        LOWER => Function->new("LOWER", 0, 1, 'lc(__0__)'),
        TRIM => Function->new("TRIM", 0, 1, 
                'do { my $temp = __0__; $temp =~ s/^\s*|\s*$//g; $temp; }'),
        TOK_ISNULL => Function->new("TOK_ISNULL", 0, 1, 
                'do { defined __0__ ? 1 : 0 }'),
        TOK_ISNOTNULL => Function->new("TOK_ISNOTNULL", 0, 1, 
                'do { defined __0__ ? 0 : 1 }'),

        #hash functions
        MD5    => Function->new("MD5", 0, 1, 'do { BEGIN { use Digest::MD5 qw(md5); }; md5(__0__); }'),
        MD5HEX => Function->new("MD5HEX", 0, 1, 'do { BEGIN { use Digest::MD5 qw(md5_hex); }; md5_hex(__0__); }'),
        BASE64 => Function->new("BASE64", 0, 1, 'do { BEGIN { use Digest::MD5 qw(md5_base64); }; md5_base64(__0__); }'),

    }, $class;
}

sub get {
    my ($self, $function_name, $distinct) = @_;
    my $name = uc($function_name) . ($distinct ? "_DI" : "");
    my $function = $self->{$name}
                    or die "Unknown function name " . uc($function_name)
                                . "(" . ($distinct ? "DISTINCT" : "") . ")";
    return $function;
}

1;
