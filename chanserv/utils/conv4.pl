#!/usr/bin/perl -w

use DBI;
use strict;

my $pathname="/home/q9test/database/";

my %chans;
my %users;
my %chanusers;

my %suspends;

my $chanid=1;
my $chanusercount=0;

$|=1;

open BANS, ">bans.csv";
open CHANS, ">chans-000.csv";
open USERS, ">users-000.csv";
open CHANUSERS, ">chanusers-000.csv";
open ORPHANS, ">orphanlist";
open SQL, ">load.sql";

print SQL "TRUNCATE TABLE channels;\n";
print SQL "TRUNCATE TABLE users;\n";
print SQL "TRUNCATE TABLE bans;\n";
print SQL "TRUNCATE TABLE chanusers;\n";
print SQL "COPY users FROM '${pathname}users-000.csv' DELIMITER ',';\n";
print SQL "COPY channels FROM '${pathname}chans-000.csv' DELIMITER ',';\n";
print SQL "COPY chanusers FROM '${pathname}chanusers-000.csv' DELIMITER ',';\n";
print SQL "COPY bans FROM '${pathname}bans.csv' DELIMITER ',';\n";

print "Converting Q channels..";
loadchans();
print "\nConverting Q users..";
loadsuspendusers();
loadusers();
print "\nConverting L channels..";
loadldb();

sub loadsuspendusers {
  open SUSPLIST, "suspended_users"  or die "Unable to open suspended users list";
  
  my $state=0;
  my $name;
  my $type;
  my $who;
  my $stime;
  my $exp;
  my $reason;
  my $unl_retries;
  my $unl_password;
  
  while (<SUSPLIST>) {
    chomp;
    
    if ($state==0) {
      $name=$_;
      $state++;
    } elsif ($state==1) {
      $type=$_;
      $state++;
    } elsif ($state==2) {
      $who=$_;
      $state++;
    } elsif ($state==3) {
      $stime=$_;
      $state++;
    } elsif ($state==4) {
      $exp=$_;
      $state++;
    } elsif ($state==5) {
      if ($_ eq "!") {
        $reason="";
      } else {
        $reason=$_;
      }
      $state++;
    } elsif ($state==6) {
      $state++;
    } elsif ($state==7) {
      $state++;
    } elsif ($state==8) {
      if ($_ ne "end") {
        die "suspended_users format errar!";
      }
      $state=0;
      $suspends{$name} = [ $type, $who, $stime, $exp, $reason ];
    }
  }
  
  close SUSPLIST;
}


sub loadchans {
  open CHANLIST, "channels"   or die "Unable to open channel list";
  
  my $state=0;
  my $cname;
  my $flags;
  my $realflags;
  my $owner;
  my $addby;
  my $suspended;
  my $suspendby;
  my $suspendtime;
  my $suspenduntil;
  my $suspendreason;
  my $type;
  my $topic;
  my $lasttopic;
  my $welcome;
  my $key;
  my $limit;
  my $firstjoin;
  my $lastjoin;
  my $joincount;
  my $banstr;
  my $banby;
  my $forcemodes;
  
  my $banid=1;
  
  while(<CHANLIST>) {
    chomp;
    if ($state==0) {
      $cname=$_;
    } elsif ($state==1) {	
      $flags=$_;
    } elsif ($state==2) {
      $owner=$_;
    } elsif ($state==3) {
      $addby=$_;
    } elsif ($state==4) {
      $suspended=$_;
    } elsif ($state==5) {
      $suspendby=$_;
    } elsif ($state==6) {
      $suspendtime=$_;
    } elsif ($state==7) {
      $suspenduntil=$_;
    } elsif ($state==8) {
      $suspendreason=$_;
      if ($suspendreason eq "!") { $suspendreason = ""; }
    } elsif ($state==9) {
      $type=$_;
    } elsif ($state==10) {
      $topic=$_;
    } elsif ($state==11) {
      $lasttopic=$_;
      if ($lasttopic eq "!") { $lasttopic = ""; }	
    } elsif ($state==12) {
      $welcome=$_;
      if ($welcome eq "!") { $welcome = ""; }
    } elsif ($state==13) {
      $key=$_;
      if ($key eq "!") { $key=""; }
      if (length $key > 23) { $key=""; }
    } elsif ($state==14) {
      $limit=$_;
      unless($limit =~ /^\d+$/) { $limit=0; }
      if ($limit > 9999) { $limit = 9999; }
    } elsif ($state==15) {
      $firstjoin=$_;
    } elsif ($state==16) {
      $lastjoin=$_;
    } elsif ($state==17) {
      $joincount=$_;
    } elsif ($state==18) {
      $banstr=$_;
    } elsif ($state==19) {
      $banby=$_;
      
      if ($banstr ne "!") {
	print BANS "$banid,$chanid,$banby,".doquote($banstr).",0,\n";
	$banid++;
	$state=17;
	if (!($banid % 1000)) {
	  close BANS;
	  my $fname=sprintf("bans-%03d.csv",$banid/1000);
	  open BANS, ">$fname";
          print SQL "COPY bans FROM '${pathname}${fname}' DELIMITER ',';\n";
	}
      } else {
	($forcemodes, $realflags)=map_chanflags($flags);
	if ($suspended) {
	  $realflags |= 0x4000;
	}
	$welcome = doquote($welcome);
	$lasttopic = doquote($lasttopic);
	print CHANS "$chanid,".doquote($cname).",$realflags,$forcemodes,0,$limit,10,0,$firstjoin,$lastjoin,$firstjoin,0,$owner,$addby,$suspendby,$suspendtime,$type,$joincount,$joincount,0,0,$welcome,$lasttopic,".doquote($key).",".doquote($suspendreason).",,0\n";
	if (($chanid % 1000)==0) {
	  close CHANS;
	  my $fname=sprintf("chans-%03d.csv",$chanid/1000);
	  open CHANS, ">$fname";
          print SQL "COPY channels FROM '${pathname}${fname}' DELIMITER ',';\n";
	}
	$chans{irc_lc($cname)}=$chanid;
	$state=-1;
	unless ($chanid % 1000) {
	  print ".";
	}
	$chanid++;
      }
    }
    $state++;
  }

  close CHANLIST;
}

sub map_chanflags {
  my ($oldflags) = @_;

  my $newflags=0x0080;
  my $forcemodes=0x3;

  if ($oldflags & 0x02) {  # +b
    $newflags |= 0x02;
  } 

  if ($oldflags & 0x04) {  # +c
    $newflags |= 0x04;
  } 

  if ($oldflags & 0x20) { # +f
    $newflags |= 0x10;
  }

  if ($oldflags & 0x8000) { # +p
    $newflags |= 0x200;  
  } 

  if ($oldflags & 0x80000) { # +t
    $newflags |= 0x800;
  }

  if ($oldflags & 0x400000) { # +w
    $newflags |= 0x2000;
  }

  if ($oldflags & 0x400) { # +k: maps to forcemode +k
    $forcemodes |= 0x40;
  }

  if ($oldflags & 0x800) { # +l: maps to forcemode +l
    $forcemodes |= 0x20;
  }

  return ($forcemodes, $newflags);
}

sub loadusers {
  open USERLIST, "users" or die "Unable to open user list";

  my $username;
  my $userid;
  my $password;
  my $globalauth;
  my $lastauth;
  my $lastyxx;
  my $lastusername;
  my $lasthostname;
  my $lastemail;
  my $emailaddr;
  my $chan;
  my $flags;
  my $uflags;
  my $realflags;
  my $usercount=0;
  my $lastemailrq;
  my $lockuntil;
  my $nuflags;
  my $state=0;
  my $suspendwho=0;
  my $suspendtime=0;
  my $suspendexp=0;
  my $suspendreason="";

  while (<USERLIST>) {
    chomp;
    
    if ($state == 0) {
      $username=$_;
    } elsif ($state == 1) {
      $userid=$_;
    } elsif ($state == 2) {
      $password=$_;
    } elsif ($state == 3) {
      $globalauth=$_;
    } elsif ($state == 4) {
      $lastauth=$_;
    } elsif ($state == 5) {
      $lastyxx=$_;
    } elsif ($state == 6) {
      $lastusername=$_;
    } elsif ($state == 7) {
      $lasthostname=$_;
    } elsif ($state == 8) {
      $lastemailrq=$_;
    } elsif ($state == 9) {
      $emailaddr=$_;
    } elsif ($state == 10) {
      $lockuntil=$_;
    } elsif ($state == 11) {
      $lastemail=$_;
    } elsif ($state == 12) {
      $nuflags=$_;
    } elsif ($state == 13) {
      if ($_ eq "end") {
#	print "Got user $username [$userid]\n";
	$uflags=4;
	if ($globalauth >= 900) {
	  $uflags |= 0x20;
	} 
	if ($globalauth > 10 and $globalauth < 900) {
	  $uflags |= 0x100;
	}
	if ($globalauth >= 1000) {
	  $uflags |= 0x40;
	}
	if ($nuflags & 8) { # No auth limit
	  $uflags |= 0x1000;
        }
        if ($nuflags & 64) { # No delete
          $uflags |= 0x4000;
        }
        if ($nuflags & 1) { # Has trust
          $uflags |= 0x8000;
        }
        if (exists $suspends{$username}) {
          my $ar=$suspends{$username};
          if ($$ar[0] == 7) { # never authed
            $uflags |= 0x1;
          } elsif ($$ar[0] == 5) { # delayed gline
            $uflags |= 0x0800;
          } elsif ($$ar[0] == 6) { # instant gline
            $uflags |= 0x2;
          } elsif ($$ar[0] == 1) { # public
            $uflags |= 0x10;
          } else {
            print "Suspend reason $$ar[0] found!\n";
            $uflags |= 0x10;
          }
          $suspendwho=$$ar[1];
          $suspendtime=$$ar[2];
          $suspendexp=$$ar[3];
          $suspendreason=doquote($$ar[4]);
        } else {
          $suspendwho=0;
          $suspendtime=0;
          $suspendexp=0;
          $suspendreason="";
        }  
	print USERS "$userid,".doquote($username).",0,$lastauth,$lastemailrq,$uflags,0,$suspendwho,$suspendexp,$suspendtime,$lockuntil,".doquote($password).",".doquote($emailaddr).",".doquote($lastemail).",".doquote("${lastusername}\@${lasthostname}").",,,\n";
	$users{irc_lc($username)}=$userid;
	$state=-1;
	$usercount++;
	unless ($usercount % 1000) {
	  print ".";
	  close USERS;
	  my $fname=sprintf("users-%03d.csv",$usercount/1000);
	  open USERS, ">$fname";
          print SQL "COPY users FROM '${pathname}${fname}' DELIMITER ',';\n";
	}
      } else {
	if (not defined $chans{irc_lc($_)}) {
	  print "Error: unknown channel $_\n";
	  $chan=0;
	} else {
	  $chan=$chans{irc_lc($_)};
	}
      }
    } elsif ($state == 14) {
      if ($chan>0) {
	$flags=$_;
	$realflags=map_chanlevflags($flags);
        my $idstr=$userid.".".$chan;
        unless (exists $chanusers{$idstr}) {
  	  print CHANUSERS "$userid,$chan,$realflags,0,0,\n";
  	  $chanusercount++;
  	  if (!($chanusercount % 10000)) {
  	    close CHANUSERS;
  	    my $fname=sprintf("chanusers-%03d.csv",$chanusercount/10000);
  	    open CHANUSERS, ">$fname";
  	    print SQL "COPY chanusers FROM '${pathname}${fname}' DELIMITER ',';\n";
          }
  	  $chanusers{$idstr}=1;
        }
      }
      $state=12;
    }
 
    $state++;
  }

  close USERLIST;
}

sub map_chanlevflags {
  my ($oldflags) = @_;

  my $newflags=0;
  
  if ($oldflags & 0x1) { # +a
    $newflags |= 0x9;
  }

  if ($oldflags & 0x2) { # +b
    $newflags |= 0x2;
  } 

  if ($oldflags & 0x1000) { # +m
    $newflags |= 0x4000;
  }
  
  if ($oldflags & 0x2000) { # +n
    $newflags |= 0x8000;
  } 

  if ($oldflags & 0x4000) { # +o
    $newflags |= 0x2000;
  }

  if ($oldflags & 0x200000) { # +v
    $newflags |= 0x1000;
  }

  if ($oldflags & 0x80000) { # +t
    $newflags |= 0x40;
  }

  if ($newflags & 0x2000) {
    $newflags &= ~0x8;
  } elsif ($newflags & 0x1000) {
    $newflags &= ~0x1;
  } else {
    $newflags &= ~0x9;
  }

  return $newflags;
}

sub loadldb {
  open LDB, "accounts.0" or die "Error opening lightweight database";

  my $state=0;
  
  my $channame;
  my $addedby;
  my $founder;
  my $lastused;
  my $added;
  my $flags;
  my $cuflags;
  my $userid;
  my $suspendby;
  my $suspendreason;
  my $cflags;
  my $welcome;
  my $curchanid;
  my $skipadd;
  my $forcemodes;

  while (<LDB>) {
    chomp;

    if ($state == 0) {
      /^--- End of/ && ($state=1);
    } elsif ($state==1) {
      /^(.+?) (.+?) (.+?) (\d+) (\d+) (\d+)$/ or next;
      $channame=$1;
      $addedby=getuserid($2);
      $founder=getuserid($3);
      $lastused=$4;
      $added=$5;
      $flags=$6;
      $suspendby=0;
      $suspendreason="";
      $welcome="";
      $forcemodes=0x3;
      $skipadd=0;

      if (defined $chans{irc_lc($channame)}) {
	$curchanid=$chans{irc_lc($channame)};
	print "Duplicate channel $channame, merging chanlev\n";
	$skipadd=1;
      } else {
	unless ($chanid % 1000) {
	  print ".";
	}
	$curchanid=$chanid;
	$chanid++;
      }

      $cflags=0x0080;

      if ($flags & 0x40) {
	$forcemodes |= 0x10;
      }

      if ($flags & 0x2) {
	$state=2;
      } elsif ($flags & 0x20) {
	$state=3;
      } else {
	$state=10;
      }
    } elsif ($state==2) {
      /^(\S+) (.+)$/ or next;

      $suspendby = $users{irc_lc($1)};
      if (not defined $suspendby) {
	$suspendby=0;
      }
      $suspendreason=doquote($2);

      $cflags |= 0x4000;
      if ($flags & 0x20) {
	$state=3;
      } else {
	$state=10;
      }
    } elsif ($state==3) {
      $cflags |= 0x2000;
      $welcome=doquote($_);
      $state=10;	
    } elsif ($state==10) {
      if (/^--- End/) {
	unless ($skipadd) {
          print CHANS "$curchanid,".doquote($channame).",$cflags,$forcemodes,0,0,10,0,$added,$lastused,$added,0,$founder,$addedby,$suspendby,0,0,0,0,0,0,$welcome,,,$suspendreason,,0\n";
          if (($chanid % 1000)==0) {
            close CHANS;
            my $fname=sprintf("chans-%03d.csv",$chanid/1000);
            open CHANS, ">$fname";
            print SQL "COPY channels FROM '${pathname}${fname}' DELIMITER ',';\n";
          }
	}
	$state=1;
      } else {
	/^(\S+)\s+(\S+)$/ or next;
	$cuflags=0;
	my $ocuflags=$2;
	$userid=getuserid($1);
	if ($userid != 0) {
	  if ($ocuflags =~ /a/) {
	    $cuflags |= 0x1;
	  }
	  if ($ocuflags =~ /g/) {
	    $cuflags |= 0x8;
	  }
	  if ($ocuflags =~ /m/) {
	    $cuflags |= 0x4000;
	  } 
	  if ($ocuflags =~ /n/) {
	    $cuflags |= 0x8000;
	  }
	  if ($ocuflags =~ /o/) {
	    $cuflags |= 0x2000;
	  }
	  if ($ocuflags =~ /v/) {
	    $cuflags |= 0x1000;
	  }
	  
          my $idstr=$userid.".".$curchanid;
          unless (exists $chanusers{$idstr}) {
            $chanusers{$idstr}=1;
            print CHANUSERS "$userid,$curchanid,$cuflags,0,0,\n"; 
            $chanusercount++;
            if (!($chanusercount % 10000)) {
              close CHANUSERS;
              my $fname=sprintf("chanusers-%03d.csv",$chanusercount/10000);
              open CHANUSERS, ">$fname";
              print SQL "COPY chanusers FROM '${pathname}${fname}' DELIMITER ',';\n";
            }
          } else {
            print "Suppressing duplicate chanuser: $idstr\n";
          }
	}
      }
    }
  }
}
	
sub irc_lc {
  my ($incoming) = @_;

  $incoming = lc $incoming;
  $incoming =~ tr/\[\]\|~∆¡¿‹–ƒ≈÷/\{\}\\^Ê·‡¸‰Âˆ/;

  return $incoming;
}

sub getuserid {
  my ($username) = @_;
  my $userid;
  
  $userid=$users{irc_lc($username)};
  if (not defined $userid) {
    print ORPHANS irc_lc($username)."\n";
    return 0;
  }
  
  return $userid;
}

sub doquote {
  my ($instr) = @_;
  
 my $outstr = "";
  
  for (split //, $instr) {
    if ($_ eq ",") {
      $outstr .= "\\,";
    } elsif ($_ eq "\\") {
      $outstr .= "\\\\";
    } else {
      $outstr .= $_;
    }
  }
  
#  $outstr .= "\'";
  
  return $outstr;
}
