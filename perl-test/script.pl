#!/usr/bin/perl -w
use Cwd;
$ProjectDir = "/home/user/workDirectory/perl-test";
$CoojaDir = "/home/user/contiki/tools/cooja/contiki_tests";
$ProjectName = "perl-test";
##List of Parameter.
##TO DO ...
@SendPeriod = (10,8,6,4);
##
=pod
foreach $value (@SendPeriod) {
	Edit_Project_Conf($value);
	system "make clean";
	system "make";
	chdir ($CoojaDir);
	system("bash RUN_TEST $ProjectName");
	system("cp $ProjectName.log $ProjectDir/output/$ProjectName$value.log");
	chdir ($ProjectDir);
}
=cut
Edit_CSC($ProjectDir);
##Edit simulation file(.csc);
sub Edit_CSC {
	my $ProjectDir = $_[0];
	##May be changed.
	my $PositionDir = $ProjectDir."/positions";
	my $file = "perl-test.csc";
	my $changeline = '<positions EXPORT="copy">';
	my @positions_files = <$PositionDir/*>;
	foreach $positionfile (@positions_files) {

		open my $read_fh, '<', $file
			or die "Couldnot open the file $file:$!";
		open my $write_fh, '+<', $file
			or die "Couldnot open the file $file:$!";

		print "####################\n";
		while($line = <$read_fh>) {
			if ($line =~ m/$changeline/) {
				print $line."\n";
				$line =~ s#>.*<#>$positionfile<#;
				print $line."\n";
			}
			print $write_fh $line;
		}
		close $read_fh;
		close $write_fh;
	}
}       

	

##Edit project configure file(project_confh.h).
sub Edit_Project_Conf {
	my $period = $_[0];
    ##May be changed.
	my $file = "project-conf.h";
	my $changeline = '#define PERIOD'; ## line 10
	
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
