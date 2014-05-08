#!/usr/bin/perl -w
use Cwd;
$ProjectDir = "/home/user/contiki/nick-projects/rpl-pfeval";
$CoojaDir = "/home/user/contiki/tools/cooja/contiki_tests";
$ProjectName = "static-100-10";
##List of Parameter.
##TO DO ...
@SendPeriod = (16,14,12,10,8,6,4,2);
##
foreach $value (@SendPeriod) {
	Edit_Project_Conf($value);
	system "make clean";
	system "make";
	chdir ($CoojaDir);
	system("bash RUN_TEST $ProjectName");
	system("cp $ProjectName.log $ProjectDir/output/$ProjectName$value.log");
	chdir ($ProjectDir);
}

##Edit project configure file(project_confh.h).
sub Edit_Project_Conf {
	my $period = $_[0];
    ##May be changed.
	my $file = "project-conf.h";
	my $changeline = '#define SEND_INTERVAL'; ## line 10
	
	my $read_fh;
	my $write_fh;
	open $read_fh, '<', $file
		or die "Could not open the file $file:$!";
	open $write_fh, '+<', $file
		or die "Could not open the file $file:$!";
	while($line = <$read_fh>) {
		if ($line =~ m/$changeline/) {
			$line =~ s#\d+#$period#;
		}
		print $write_fh $line;
	}
	close $read_fh;
	close $write_fh;
}
