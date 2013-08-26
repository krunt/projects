#!/usr/bin/perl -l
use strict;
use warnings;

use Re::Re (); use Data::Dumper;

sub assert {
    my ($condition, $string) = @_;
    unless ($condition) {
        die $string;
    }
}

sub test_match {
    my ($x, $cnt) = @_;
    my ($string, $re, $result) = @$x;
    my $y = Re::Re->new($re);
    assert($y->match($string) == $result, Dumper($x));
    printf("%03d: OK\n", $cnt);
}

sub test_match_with_groups {
    my ($x, $cnt) = @_;
    my ($string, $re, @mas) = @$x;
    my $y = Re::Re->new($re);
    assert($y->match($string) == 1, Dumper($x));
    while (my ($s1, $s2) = splice(@mas, 0, 2)) {
        assert($y->paren_count() > $s1, $y->paren_count()  . "> $s1");
        assert($y->group($s1) eq $s2, $y->group($s1) . "!=" . $s2);
    }
    printf("%03d: OK\n", $cnt);
}

{
    my $test_string = ' http3 w  httpabaaaabb1a ';
    my @test_cases = (
        [ $test_string, 'http', 1 ],
        [ $test_string, '\d', 1 ],
        [ $test_string, '\d+', 1 ],
        [ $test_string, '\d*', 1 ],
        [ $test_string, '\w*', 1 ],
        [ $test_string, '\w?', 1 ],
        [ $test_string, '^ http', 1 ],
        [ $test_string, '^ \w*', 1 ],
        [ $test_string, ' $', 1 ],
        [ $test_string, '^$', 0 ],
        [ $test_string, 'a+', 1 ],
        [ $test_string, 'b+', 1 ],
        [ $test_string, 's+', 0 ],
        [ $test_string, 'a?b?a?b?a?b?a?b?a?b?', 1 ],
        [ $test_string, 'http[1-3]', 1 ],
        [ $test_string, 'http[4-5]', 0 ],
        [ $test_string, '\w+\w+\w+\w+\w+\w+', 1 ],
        [ $test_string, '\w+\w+\d+', 1 ],
        [ $test_string, '\w+\w+\d+ \w+  \w+', 1 ],
        [ $test_string, 'a+?a+?a+?', 1 ],
        [ '2a212ra1', '2[^a]', 1 ],
        [ '22212r11', '[^12]', 1 ],
        [ '22212r11', '(\w+)', 1 ],
        [ '22212r11', '(11)', 1 ],
        [ '22212r11', '(2.?2)', 1 ],
        [ '22212r11', '2(?=2)2', 1 ],
        [ '22212r11', '2(?=2+2+2+)2', 0 ],
        [ '22212r11', '(?=222)1', 0 ],
        [ '22212r11', '\w*11', 1 ],
        [ '22212r11', '\w*111', 0 ],
        [ '22212r11', '.*', 1 ],
        [ '22212r11', '^(?=222)2\w*11', 1 ],
        [ $test_string, 'httx|abb1a1|abb1a', 1 ],
        [ $test_string, 'a{4}', 1 ],
        [ 'aa', 'a{2,}', 1 ],
        [ 'aac', 'a{2,}', 1 ],
        [ 'aac', 'a{3,}', 0 ],
        [ 'baac', 'a{3,}', 0 ],
        [ 'baac', 'a{3}', 0 ],
        [ 'baacaaad', 'a{3}', 1 ],
        [ 'baacaaad', 'a{3,6}', 1 ],
        [ 'baabaabaaaacad', 'a{3,6}', 1 ],
        [ 'baaabaaabaaaacad', 'a{5,}', 0 ],
        [ 'baaabaaabaaaacad', 'a{5,6}', 0 ],
        [ 'baaabaaabaaaacad', 'a{5,5}', 0 ],
        [ $test_string, 'a{5}', 0 ],
        [ $test_string, '(a{3})', 1 ],
        [ $test_string, '(t{2})', 1 ],
        [ 'abccd', '(\w)\1', 1 ],
        [ 'abccd', '(?:r|m|s|a|q|z|d)', 1 ],
        [ 'xbc', 'abc', 0 ],
        [ 'axc', 'abc', 0 ],
        [ 'abx', 'abc', 0 ],
        [ 'abc', 'ab+bc', 0 ],
        [ 'abq', 'ab+bc', 0 ],
        [ 'abq', 'ab{1,}bc', 0 ],
        [ 'abbbbc', 'ab{4,5}bc', 0 ],
        [ 'abbbbc', 'ab?bc', 0 ],
        [ 'abcc', '^abc$', 0 ],
        [ 'aabc', '^abc$', 0 ],
        [ 'aabcd', 'abc$', 0 ],
        [ 'axyzd', 'a.*c', 0 ],
        [ 'abc', 'a[bc]d', 0 ],
        [ 'aBd', '.[b].', 0 ],
        [ 'abd', 'a[b-d]e', 0 ],
        [ 'abd', 'a[^bc]d', 0 ],
        [ 'adcdcde', 'a[bcd]+dcdcde', 0 ],
        [ 'effg', '(bc+d$|ef*g.|h?i(j|k))', 0 ],
        [ 'bcdd', '(bc+d$|ef*g.|h?i(j|k))', 0 ],
        [ 'uh-uh', 'multiple words of text', 0 ],
        [ 'bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+?)+?X', 1 ],

#        slow tests
#        [ 'bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+X', 1 ],
#        [ 'bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+XX', 1 ],
#        [ 'bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.XX(.+)+X', 1 ],
#        [ 'bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+X', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+XX', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.XX(.+)+X', 0 ],
#        [ 'bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+[X]', 1 ],
#        [ 'bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+[X][X]', 1 ],
#        [ 'bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.XX(.+)+[X]', 1 ],
#        [ 'bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+[X]', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.X(.+)+[X][X]', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.XX(.+)+[X]', 0 ],
#        [ 'bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X](.+)+[X]', 1 ],
#        [ 'bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X](.+)+[X][X]', 1 ],
#        [ 'bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X][X](.+)+[X]', 1 ],
#        [ 'bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X](.+)+[X]', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X](.+)+[X][X]', 0 ],
#        [ 'bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '.[X][X](.+)+[X]', 0 ],

        [ 'hello', '(?>l)l', 1 ],
        [ 'helll', '(?>l*)l', 0 ],
        [ 'lll', '(?>l+)l+', 0 ],
        [ 'lll', '(?>l*)l*', 1 ],

        [ 'all', '(?!a)l', 1 ],
        [ 'albl', 'l(?!a)l', 0 ],

        [ 'albl', 'l(?<=l)bl', 1 ],
        [ 'albl', 'l(?<=\d)bl', 0 ],
        [ 'al9bl', '(?<=\d)bl', 1 ],
        [ 'albl', '(?<=\d)bl', 0 ],
    );
    
    my $cnt = 1;
    for my $test_case (@test_cases) {
        test_match($test_case, $cnt++);
    }
}


{
    my $test_string = ' http3 w  httpabaaaabb1a ';
    my @test_cases = (
        [ $test_string, 'http', 0, 'http' ],
        [ $test_string, 'http(\d)', 1, '3' ],
        [ $test_string, ' (\w) ', 0, ' w ', 1, 'w' ],
        [ $test_string, '(a*)', 1, '' ],
        [ $test_string, '(a+)\w(a+)', 1, 'a', 2, 'aaaa' ],
        [ $test_string, '(a+)\w(a+?)', 1, 'a', 2, 'a' ],
        [ $test_string, 'a*(aaabb)1', 1, 'aaabb' ],
        [ $test_string, 'h(t{2})p3', 1, 'tt' ],
        [ $test_string, '(a{4})', 1, 'aaaa' ],
        [ $test_string, '((?:(aa{1})){2})', 1, 'aaaa', ],
        [ 'xa1212baby', '((?:\d\d){2})', 1, '1212', ],
        [ 'abccd', '(\w)\1', 1, 'c' ],
        [ 'xxabcbcbcd', '(\w\w)\1\1', 1, 'bc' ],
        [ $test_string, '(http).*\1', 1, 'http' ],
        [ 'abc', 'abc', 0, 'abc' ],
        [ 'xabcy', 'abc', 0, 'abc' ],
        [ 'ababc', 'abc', 0, 'abc' ],
        [ 'abc', 'ab*c', 0, 'abc' ],
        [ 'abc', 'ab*bc', 0, 'abc' ],
        [ 'abbc', 'ab*bc', 0, 'abbc' ],
        [ 'abbbbc', 'ab*bc', 0, 'abbbbc' ],
        [ 'abbbbc', '.{1}', 0, 'a' ],
        [ 'abbbbc', '.{3,4}', 0, 'abbb' ],
        [ 'abbbbc', 'ab{0,}bc', 0, 'abbbbc' ],
        [ 'abbc', 'ab+bc', 0, 'abbc' ],
        [ 'abbbbc', 'ab+bc', 0, 'abbbbc' ],
        [ 'abbbbc', 'ab{1,}bc', 0, 'abbbbc' ],
        [ 'abbbbc', 'ab{1,3}bc', 0, 'abbbbc' ],
        [ 'abbbbc', 'ab{3,4}bc', 0, 'abbbbc' ],
        [ 'abbc', 'ab?bc', 0, 'abbc' ],
        [ 'abc', 'ab?bc', 0, 'abc' ],
        [ 'abc', 'ab{0,1}bc', 0, 'abc' ],
        [ 'abc', 'ab?c', 0, 'abc' ],
        [ 'abc', 'ab{0,1}c', 0, 'abc' ],
        [ 'abc', '^abc$', 0, 'abc' ],
        [ 'abcc', '^abc', 0, 'abc' ],
        [ 'aabc', 'abc$', 0, 'abc' ],
        [ 'abc', '^', 0, '' ],
        [ 'abc', '$', 0, '' ],
        [ 'abc', 'a.c', 0, 'abc' ],
        [ 'axc', 'a.c', 0, 'axc' ],
        [ 'axyzc', 'a.*c', 0, 'axyzc' ],
        [ 'abd', 'a[bc]d', 0, 'abd' ],
        [ 'abd', 'a[b]d', 0, 'abd' ],
        [ 'abd', '[a][b][d]', 0, 'abd' ],
        [ 'abd', '.[b].', 0, 'abd' ],
        [ 'abd', '(?:.[b].)', 0, 'abd' ],
        [ 'ace', 'a[b-d]e', 0, 'ace' ],
        [ 'aac', 'a[b-d]', 0, 'ac' ],
        [ 'aed', 'a[^bc]d', 0, 'aed' ],
        [ 'abc', 'ab|cd', 0, 'ab' ],
        [ 'abcd', 'ab|cd', 0, 'ab' ],
        [ 'aabbabc', 'a+b+c', 0, 'abc' ],
        [ 'aabbabc', 'a{1,}b{1,}c', 0, 'abc' ],
        [ 'abcabc', 'a.+?c', 0, 'abc' ],
        [ 'cde', '[^ab]*', 0, 'cde' ],
        [ '', 'a*', 0, '' ],
        [ 'e', 'a|b|c|d|e', 0, 'e' ],
        [ 'abcdefg', 'abcd*efg', 0, 'abcdefg' ],
        [ 'xabyabbbz', 'ab*', 0, 'ab' ],
        [ 'xayabbbz', 'ab*', 0, 'a' ],
        [ 'hij', '[abhgefdc]ij', 0, 'hij' ],
        [ 'adcdcde', 'a[bcd]*dcdcde', 0, 'adcdcde' ],
        [ 'alpha', '[a-zA-Z_][a-zA-Z0-9_]*', 0, 'alpha' ],
        [ 'aac', '(((((((((a)))))))))\9', 0, 'aa' ],
        [ 'a', '(((((((((a)))))))))', 0, 'a' ],
    );
    
    my $cnt = 1;
    for my $test_case (@test_cases) {
        test_match_with_groups($test_case, $cnt++);
    }
}
