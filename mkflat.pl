#!/usr/bin/perl -w

# mkflat.pl: Makes a flat, statically linked version of newserv

my @coredirs = ( "core", "parser", "lib" );
my @moduledirs;
my $configfile="newserv.conf";
my $flatdir="flat";

if (defined $ARGV[0]) {
  $configfile=$ARGV[0];
}

open CFG, $configfile    or die "Unable to open $configfile: $!";

my $insection=0;

while (<CFG>) {
  chomp;
  
  if (/\[core\]/) {
    $insection=1;
    next;
  } elsif (/\[\.*\]/) {
    $insection=0;
    next;
  } else {
    if ($insection && m/^loadmodule=(.*)$/) {
      push @moduledirs, $1;
    }
  }
}

close CFG;

open NEWSERVH,">", $flatdir."/newserv.h"    or die "Unable to open flat header: $!";
open NEWSERVC,">", $flatdir."/newserv.c"    or die "Unable to open flat file  : $!";

print NEWSERVC '#include "newserv.h"'."\n\n";

foreach (@coredirs) {
  my $dir=$_;
  opendir DIR, $_;
  my @files=readdir DIR;
  closedir DIR;

  foreach (@files) {
    if (/\.[ch]$/) {
      dofile($dir, $_, undef);
    }
  }
}

foreach (@moduledirs) {
  my $dir=$_;
  opendir DIR, $_;
  my @files=readdir DIR;
  closedir DIR;

  foreach (@files) {
    if (/\.[ch]$/) {
      dofile($dir, $_, $dir);
    }
  }
}
  

sub dofile {
  my ($dirname, $filename, $modname) = @_;
 
  open INFILE, $dirname."/".$filename       or die "Unable to open file: $!";

  if ($filename =~ /h$/) {
    # header file

    while(<INFILE>) {
      print NEWSERVH $_;
    }
  } else {
    # .c file
    
    open OUTFILE, ">", $flatdir."/".$dirname."_".$filename or die "Unable to open file $flatdir/${dirname}_$filename: ";

#    print OUTFILE '#include "newserv.h"'."\n\n";
    
    while (<INFILE>) {
#      next if (/^#include \"/);   # Skip local include files

      if (/void _init\(\)/ && defined $modname) {
        s/_init/${modname}_init/;
      }

      if (/void _fini\(\)/ && defined $modname) {
        s/_fini/${modname}_fini/;
      }

      if (/initmodules\(\);/) {
        foreach (@moduledirs) {
          print OUTFILE $_."_init\(\);";
          print NEWSERVC $_."_init\(\);";
        }
        next;
      }

      print OUTFILE $_;
      print NEWSERVC $_;
    }

    close OUTFILE;
  }        
}  