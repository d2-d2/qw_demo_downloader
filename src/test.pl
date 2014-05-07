#!/usr/bin/perl
$string = "1340: duel_senft_vs_phanter[aerowalk].mvd 920k";
$string = "1: duel_.watch_my_ping._vs_(klapcior)[dm2]2007-12-17.mvd 10k";

if ($string =~ m/(\d+): (.+) (\d+k)/){
	print "string: \"$string\"\n";
	print "id: $1" . " " . "name: $2" . " " . "size: $3" . "\n";
}else{
	print "string: \"$string\"\n";
	print "noes\n";
}

