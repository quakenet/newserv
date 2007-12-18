#!/usr/bin/perl -w

use strict;

my $verbose=0;

my %definers;
my %reqsyms;
my %reqmods;
my %printed;

my @arglist = <*.so>;
#push @arglist, "../newserv";
my @realarglist;

open DEPENDS,">modules.dep";
open GRAPH,">modgraph.dot";

if (@ARGV) {
  if ($ARGV[0] eq "--verbose" or $ARGV[0] eq "-v") {
    $verbose=1;
  }
}

for (@arglist) {
  my $modname=$_;
  open NM,"nm $modname |";
  
  $modname =~ s/.so$//;
  
  push @realarglist, $modname;
  
  $reqsyms{$modname} = [];
  
  while (<NM>) {
    chomp;
    next unless (/([URTB]) (\S+)/);

    my ($type,$sym) = ($1, $2);
    
    next if ($sym eq "_init");
    next if ($sym eq "_fini");
    next if ($sym eq "_version");

    if ($type eq "U") {
      push @{$reqsyms{$modname}}, $sym;
    } else {
      if (defined $definers{$sym}) {
        print "WARNING: Multiple modules export $sym, at least $modname and $definers{$sym}\n";
      } else {
        $definers{$sym}=$modname;
      }
    }
  }
  
  close NM;
}

print GRAPH "digraph g { \n";

for (@realarglist) {
  my $modname=$_;
  
  $reqmods{$modname} = {};
  
  for (@{$reqsyms{$modname}}) {
    my $provider = $definers{$_};
    
    if (defined $provider) {
      ${reqmods{$modname}}{$provider}=1;
      if ($verbose) {
        print "Module $modname importing $_ from $provider\n";
      }
    }
  }
}

for (@realarglist) {
  printdep($_);
}

print GRAPH "}\n";

close GRAPH;
close DEPENDS;

sub printdep {
  my ($modname) = @_;
  
  if (!(defined $printed{$modname})) {
    $printed{$modname}=1;
    
    for (keys %{$reqmods{$modname}}) {
      printdep($_);
    }

    print DEPENDS "$modname";
    for (keys %{$reqmods{$modname}}) {
      if (defined ${$reqmods{$_}}{$modname}) {
        print "ERROR: circular dependency between $modname and $_\n";
      }
      print DEPENDS " $_";
      print GRAPH "\t\"$modname\" -> \"$_\";\n";
    }
    print DEPENDS "\n";
  }
}
