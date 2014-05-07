#use strict;
#use IO::Socket::INET;
#
#
sub init {
	our @console_lines;
}



sub frame {
	my ($time)  = @_;

}

sub addtoconsole {
	my ($string) = @_;
	$string =~ s/\n//g;
	$string =~ s/\r//g;
	push(@console_lines,$string);
}

sub call_function {
	$func = shift;
	if (defined(&$func)){
		&$func(@_);
	}else{
		print "function \"$func\" not defined\n";
	}
}


	

