#!/usr/local/bin/perl
#
use DBI;

my $dbh = DBI->connect("dbi:Sybase:server=JDBC", 'guest', 'sybase', {PrintError => 0});

die "Unable for connect to server $DBI::errstr"
    unless $dbh;

my $rc;
my $sth;

$sth = $dbh->prepare("select \@\@version");
if($sth->execute) {
    while(@dat = $sth->fetchrow) {
		print "@dat\n";
    }
}
