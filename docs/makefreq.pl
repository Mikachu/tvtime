#!/usr/bin/perl -w
######################################
#
#  Copyright (C) 2002 Achim Schneider <batchall@mordor.ch>
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or (at
#  your option) any later version.
# 
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 


my $root= "/usr/share/xawtv/";


sub getFile {
	(my $table, my $file, my $handle) = @_;
	$handle ||= "dh00";
	++$handle;
	open $handle, $root.$file or die "can't open $file";
	while (<$handle>) {
		/^$/ and next;
		if (/^#include "(.*)"\s*$/) {
			&getFile($table,$1,$handle);
			next;
		}
		
		/^#/ and next;
		
		if (/^\[(.+)\]$/) {
			$chan= $1;
			<$handle> =~ /freq\s*=\s*(\S+)\s*$/;
			$channels{$chan}{$table}= $1;
		}

		
		
	}
	close $handle;
}


open INDEX, $root . "Index.map" or die "can't open Index.map"; 

$i = 0;
while (<INDEX>) {
	/^$/ and next;
	/^#/ and next;
	if (/^\[(.*)\]$/) {
		$table= $1;
		<INDEX> =~ /file\s*=\s*(\S+)\s*$/;
		$tables[$i++] = $table;
		&getFile($table,$1);
		
	}
}

print <<EOF;
struct table_info {
  const char *long_name;
  const char *short_name;
};
EOF
print "struct table_info freq_table_names[] = {\n";
$i = 0;
foreach $tab (@tables) {
	++$i;
	print "{\"$tab\", \"$tab\" },\n";
}
print "};\n#define NUM_FREQ_TABLES $i\n";



print <<EOF;
struct freqlist {
  const char name[4];
  int freq[NUM_FREQ_TABLES];
};
EOF
print "struct freqlist tvtuner[] = {\n";
$i = 0;
foreach $chan (sort keys %channels) {
	++$i;
	print "{\"$chan\",{";
	foreach $tab (@tables) {
		print (($channels{$chan}{$tab} || 0).",");
	}
	print "}},\n";
}
print "};\n#define CHAN_ENTRIES $i\n";
