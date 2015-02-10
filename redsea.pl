#!/usr/bin/perl

# redsea RDS decoder (c) Oona Räisänen OH2EIQ
#
#
# Page numbers refer to IEC 62106, Edition 2
#

use 5.010;
use strict;
use warnings;
use utf8;
no warnings 'experimental::smartmatch';

use Encode 'decode';
use Getopt::Std;
use POSIX qw/strftime/;

binmode(STDOUT, ":encoding(UTF-8)");

$| ++;

my $fs = 250e3;
my $fc = 57e3;

# Booleans
use constant FALSE => 0;
use constant TRUE  => 1;

# Offset word order
use constant {
  A  => 0,
  B  => 1,
  C  => 2,
  Ci => 3,
  D  => 4,
};

# Bit masks
use constant {
  _5BIT  => 0x000001F,
  _10BIT => 0x00003FF,
  _16BIT => 0x000FFFF,
  _26BIT => 0x3FFFFFF,
  _28BIT => 0xFFFFFFF,
};


my $correct_all = FALSE;

# Some terminal control chars
use constant   RESET => "\x1B[0m";
use constant REVERSE => "\x1B[7m";

my @GrpBuffer;
my @GrpData;
my @rcvd;
my @errblock;
my %options;
my %stn;

my @countryISO, my @groupname, my @ptynamesUS, my @ptynames;
my @TAtext, my @TPtext, my @langname, my %oda_app, my @charbasic;
my @rtpclass;
my $BlkPointer;
my $newpi, my $ednewpi;

my $pi = 0;
my $insync = FALSE;
my $verbosity = 0;
my $expofs;

my $interactive = (-t STDOUT ? TRUE : FALSE);

my $bitpipe;
my $linebuf;

commands();

initdata();

get_groups();

sub commands {

  if (!-e "rtl_redsea") {
    print "Looks like rtl_redsea isn't compiled. To fix that, please run:\n\ngcc -std=gnu99 -o rtl_redsea rtl_redsea.c -lm\n";
    exit();
  }

  getopts("lst", \%options);

  if (exists $options{h} || ($ARGV[0] // "") !~ /^[\d\.]+[kMG]?$/) {
    print
       "Usage: perl $0 [-hlst] FREQ\n\n".
       "    -h       display this help and exit\n".
       "    -l       print groups in long format\n".
       "    -s       print groups in short format (default)\n".
       "    -t       print an ISO timestamp before each group\n".
       "    FREQ     station frequency in Hz, can be SI suffixed (e.g. 94.0M)\n";
    exit();
  }

  if (exists $options{l}) {
    $verbosity = 1;
  }

  my $fmfreq = $ARGV[0];
  if ($fmfreq =~ /^([\d\.]+)([kMG])$/) {
    my %si = ( "k" => 1e3, "M" => 1e6, "G" => 1e9 );
    $fmfreq = $1 * $si{$2};
  }

  open ($bitpipe, "-|", sprintf("rtl_fm -f %.1f -M fm -l 0 -A std -s %.1f | ".
                        "sox -c 1 -t .s16 -r %.1f - -t .s16 - sinc %.1f-%.1f ".
                        "gain 15 2>/dev/null | ./rtl_redsea", $fmfreq, $fs, $fs, $fc-3500,
                        $fc+3500));
}

# Next bit from radio
sub get_bit {
  read($bitpipe, my $a, 1) or die("End of stream");
  return $a;
}

# Calculate the syndrome of a 26-bit vector
sub syndrome {
  my $vector = shift;

  my ($l, $bit);
  my $SyndReg = 0x000;

  for my $k (reverse(0..25)) {
    $bit      = ($vector  & (1 << $k));
    $l        = ($SyndReg & 0x200);      # Store lefmost bit of register
    $SyndReg  = ($SyndReg << 1) & 0x3FF; # Rotate register
    $SyndReg ^= 0x31B if ($bit);         # Premultiply input by x^325 mod g(x)
    $SyndReg ^= 0x1B9 if ($l);           # Division mod 2 by g(x)
  }

  return $SyndReg;
}


# When a block has uncorrectable errors, dump the group received so far
sub blockerror {
  my $datalen = 0;

  if ($rcvd[A]) {
    $datalen = 1;

    if ($rcvd[B]) {
      $datalen = 2;

      if ($rcvd[C] || $rcvd[Ci]) {
        $datalen = 3;
      }
    }
    my @newgrp;
    @newgrp = @GrpData[0..$datalen-1];
    push (@GrpBuffer, \@newgrp);
  } elsif ($rcvd[Ci]) {
    my @newgrp;
    @newgrp = $GrpData[2];
    push (@GrpBuffer, \@newgrp);
  }

  $errblock[$BlkPointer % 50] = TRUE;

  my $erbloks  = 0;
  for (@errblock) {
    $erbloks += ($_ // 0)
  }

  # Sync is lost when >45 out of last 50 blocks are erroneous (C.1.2)
  if ($insync && $erbloks > 45) {
    $insync   = FALSE;
    @errblock = ();
  }

  @rcvd = ();
}

sub get_groups {

  my $block = my $wideblock = my $bitcount = my $prevbitcount = 0;
  my ($dist, $message);
  my $pi = my $i = 0;
  my $j = my $datalen = my $buf = my $prevsync = 0;
  my $lefttoread = 26;
  my @syncb;

  my @offset    = (0x0FC, 0x198, 0x168, 0x350, 0x1B4);
  my @ofs2block = (0,1,2,2,3);
  my ($SyndReg,$err);
  my @ErrLookup;

  # Generate error vector lookup table for all correctable errors
  for my $shft (0..15) {
    $err = 1 << $shft;
    $ErrLookup[&syndrome(0x00004b9 ^ ($err<<10))] = $err;
  }
  for my $shft (0..14) {
    $err = 2 << $shft;
    $ErrLookup[&syndrome(0x00004b9 ^ ($err<<10))] = $err;
  }

  if ($correct_all) {
    for (my $patt = 0x01; $patt <= 0x1F; $patt += 2) {
      for my $i (0..16-int(log2($patt)+1)) {
        $err = $patt << $i;
        $ErrLookup[&syndrome(0x00005b9 ^ ($err<<10))] = $err;
      }
    }
  }

  while (TRUE) {

    # Compensate for clock slip corrections
    $bitcount += 26-$lefttoread;

    # Read from radio
    for ($i=0; $i < ($insync ? $lefttoread : 1); $i++, $bitcount++) {
      $wideblock = ($wideblock << 1) + get_bit();
    }

    $lefttoread = 26;
    $wideblock &= _28BIT;

    $block = ($wideblock >> 1) & _26BIT;

    # Find the offsets for which the syndrome is zero
    $syncb[$_] = (syndrome($block ^ $offset[$_]) == 0) for (A .. D);

    # Acquire sync

    if (!$insync) {

      if ($syncb[A] | $syncb[B] | $syncb[C] | $syncb[Ci] | $syncb[D]) {

        BLOCKS:
        for my $bnum (A .. D) {
          if ($syncb[$bnum]) {
            $dist = $bitcount - $prevbitcount;

            if (   $dist % 26 == 0
                && $dist <= 156
                && ($ofs2block[$prevsync] + $dist/26) % 4 == $ofs2block[$bnum]) {
              $insync = TRUE;
              $expofs = $bnum;
              last BLOCKS;
            } else {
              $prevbitcount = $bitcount;
              $prevsync     = $bnum;
            }
          }
        }
      }
    }

    # Synchronous decoding

    if ($insync) {

      $BlkPointer ++;

      $message = $block >> 10;

      # If expecting C but we only got a Ci sync pulse, we have a Ci block
      $expofs = Ci if ($expofs == C && !$syncb[C] && $syncb[Ci]);

      # If this block offset won't give a sync pulse
      if (!$syncb[$expofs]) {

        # If it's a correct PI, the error was probably in the check bits and hence is ignored
        if      ($expofs == A && $message == $pi && $pi != 0) {
          $syncb[A]  = TRUE;
        } elsif ($expofs == C && $message == $pi && $pi != 0) {
          $syncb[Ci] = TRUE;
        }

        # Detect & correct clock slips (C.1.2)

        elsif   ($expofs == A && $pi != 0 && ( ($wideblock >> 12) & _16BIT ) == $pi) {
          $message     = $pi;
          $wideblock >>= 1;
          $syncb[A]    = TRUE;
        } elsif ($expofs == A && $pi != 0 && ( ($wideblock >> 10) & _16BIT ) == $pi) {
          $message     = $pi;
          $wideblock   = ($wideblock << 1) + get_bit();
          $syncb[A]    = TRUE;
          $lefttoread  = 25;
        }

        # Detect & correct burst errors (B.2.2)

        $SyndReg = syndrome($block ^ $offset[$expofs]);

        if (defined $ErrLookup[$SyndReg]) {
          $message        = ($block >> 10) ^ $ErrLookup[$SyndReg];
          $syncb[$expofs] = TRUE;
        }

        # If still no sync pulse
        blockerror() if (!$syncb[$expofs]);
      }

      # Error-free block received
      if ($syncb[$expofs]) {

        $GrpData[$ofs2block[$expofs]] = $message;
        $errblock[$BlkPointer % 50]   = FALSE;
        $rcvd[$expofs]                = TRUE;

        $pi = $message if ($expofs == A);

        # If a complete group is received
        if ($rcvd[A] && $rcvd[B] && ($rcvd[C] || $rcvd[Ci]) && $rcvd[D]) {
          decodegroup(@GrpData);
        }
      }

      # The block offset we're expecting next
      $expofs = ($expofs == C ? D : ($expofs + 1) % 5);

      if ($expofs == A) {
        @rcvd = ();
      }
    }
  }
  return FALSE;
}


sub decodegroup {

  return if ($_[0] == 0);
  my ($gtype, $fullgtype);

  $ednewpi = ($newpi // 0);
  $newpi   = $_[0];

  if (exists $options{t}) {
    my $stamp = strftime("%Y-%m-%dT%H:%M:%S%z ", localtime);
    utter ($stamp, $stamp);
  }
  
  if (@_ >= 2) {
    $gtype      = bits($_[1], 11, 5);
    $fullgtype  = bits($_[1], 12, 4) . ( bits($_[1], 11, 1) ? "B" : "A" );
  } else {
    utter ("(PI only)","");
  }
  
  utter (("  PI:     ".sprintf("%04X",$newpi) .((exists($stn{$newpi}{'chname'})) ? " ".$stn{$newpi}{'chname'} : ""),
      sprintf("%04X",$newpi)));

  # PI is repeated -> confirmed
  if ($newpi == $ednewpi) {

    # PI has changed from last confirmed
    if ($newpi != ($pi // 0)) {
      $pi = $newpi;
      screenReset();
      if (exists $stn{$pi}{'presetPSbuf'}) {
        ($stn{$pi}{'PSmarkup'} = $stn{$pi}{'presetPSbuf'}) =~ s/&/&amp;/g;
        $stn{$pi}{'PSmarkup'}  =~ s/</&lt;/g;
      }
    }

  } elsif ($newpi != ($pi // 0)) {
    utter ("          (repeat will confirm PI change)","?\n");
    return;
  }

  # Nothing more to be done for PI only
  if (@_ == 1) {
    utter ("\n","\n");
    return;
  }
  
  utter (
   (@_ == 4 ? "Group $fullgtype: $groupname[$gtype]" : "(partial group $fullgtype, ".scalar(@_)." blocks)"),
   (@_ == 4 ? sprintf(" %3s",$fullgtype) : sprintf(" (%3s)",$fullgtype)));
 
  # Traffic Program (TP)
  $stn{$pi}{'TP'} = bits($_[1], 10, 1);
  utter ("  TP:     $TPtext[$stn{$pi}{'TP'}]"," TP:$stn{$pi}{'TP'}");
	
  # Program Type (PTY)
  $stn{$pi}{'PTY'} = bits($_[1], 5, 5);

  if (exists $stn{$pi}{'ECC'} && ($countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] // "") =~ /us|ca|mx/) {
    $stn{$pi}{'PTYmarkup'} = $ptynamesUS[$stn{$pi}{'PTY'}];
    utter ("  PTY:    ". sprintf("%02d",$stn{$pi}{'PTY'})." $ptynamesUS[$stn{$pi}{'PTY'}]",
           " PTY:".sprintf("%02d",$stn{$pi}{'PTY'}));
  } else {
    $stn{$pi}{'PTYmarkup'} = $ptynames[$stn{$pi}{'PTY'}];
    utter ("  PTY:    ". sprintf("%02d",$stn{$pi}{'PTY'})." $ptynames[$stn{$pi}{'PTY'}]",
           " PTY:".sprintf("%02d",$stn{$pi}{'PTY'}));
  }
  $stn{$pi}{'PTYmarkup'} =~ s/&/&amp;/g;

  # Data specific to the group type

  given ($gtype) {
    when (0)  { Group0A (@_); }
    when (1)  { Group0B (@_); }
    when (2)  { Group1A (@_); }
    when (3)  { Group1B (@_); }
    when (4)  { Group2A (@_); }
    when (5)  { Group2B (@_); }
    when (6)  { exists ($stn{$pi}{'ODAaid'}{6})  ? ODAGroup(6, @_)  : Group3A (@_); }
    when (8)  { Group4A (@_); }
    when (10) { exists ($stn{$pi}{'ODAaid'}{10}) ? ODAGroup(10, @_) : Group5A (@_); }
    when (11) { exists ($stn{$pi}{'ODAaid'}{11}) ? ODAGroup(11, @_) : Group5B (@_); }
    when (12) { exists ($stn{$pi}{'ODAaid'}{12}) ? ODAGroup(12, @_) : Group6A (@_); }
    when (13) { exists ($stn{$pi}{'ODAaid'}{13}) ? ODAGroup(13, @_) : Group6B (@_); }
    when (14) { exists ($stn{$pi}{'ODAaid'}{14}) ? ODAGroup(14, @_) : Group7A (@_); }
    when (18) { exists ($stn{$pi}{'ODAaid'}{18}) ? ODAGroup(18, @_) : Group9A (@_); }
    when (20) { Group10A(@_); }
    when (26) { exists ($stn{$pi}{'ODAaid'}{26}) ? ODAGroup(26, @_) : Group13A(@_); }
    when (28) { Group14A(@_); }
    when (29) { Group14B(@_); }
    when (31) { Group15B(@_); }

    default   { ODAGroup($gtype, @_); }
  }
 
  utter("\n","\n");

}

# 0A: Basic tuning and switching information

sub Group0A {

  # DI
  my $DI_adr = 3 - bits($_[1], 0, 2);
  my $DI     = bits($_[1], 2, 1);
  parseDI($DI_adr, $DI);

  # TA, M/S
  $stn{$pi}{'TA'} = bits($_[1], 4, 1);
  $stn{$pi}{'MS'} = bits($_[1], 3, 1);
  utter ("  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]", " TA:$stn{$pi}{'TA'}");
  utter ("  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}],      " MS:".qw(S M)[$stn{$pi}{'MS'}]);

  $stn{$pi}{'hasMS'} = TRUE;

  if (@_ >= 3) {
    # AF
    my @af;
    for (0..1) {
      $af[$_] = parseAF(TRUE, bits($_[2], 8-$_*8, 8));
      utter ("  AF:     $af[$_]"," AF:$af[$_]");
    }
    if ($af[0] =~ /follow/ && $af[1] =~ /Hz/) {
      ($stn{$pi}{'freq'} = $af[1]) =~ s/ ?[kM]Hz//;
    }
  }

  if (@_ == 4) {
      
    # Program Service Name (PS)

    if ($stn{$pi}{'denyPS'}) {
      utter ("          (Ignoring changes to PS)"," denyPS");
    } else {
      set_ps_khars($pi, bits($_[1], 0, 2) * 2, bits($_[3], 8, 8), bits($_[3], 0, 8));
    }
  }
}

# 0B: Basic tuning and switching information

sub Group0B {
  
  # Decoder Identification
  my $DI_adr = 3 - bits($_[1], 0, 2);
  my $DI     = bits($_[1], 2, 1);
  parseDI($DI_adr, $DI);

  # Traffic Announcements, Music/Speech
  $stn{$pi}{'TA'} = bits($_[1], 4, 1);
  $stn{$pi}{'MS'} = bits($_[1], 3, 1);
  utter ("  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]", " TA:$stn{$pi}{'TA'}");
  utter ("  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}],      " MS:".qw( S M)[$stn{$pi}{'MS'}]);

  $stn{$pi}{'hasMS'} = TRUE;

  if (@_ == 4) {
      
    # Program Service name

    if ($stn{$pi}{'denyPS'}) {
      utter ("          (Ignoring changes to PS)"," denyPS");
    } else {
      set_ps_khars($pi, bits($_[1], 0, 2) * 2, bits($_[3], 8, 8), bits($_[3], 0, 8));
    }
  }

}
  
# 1A: Program Item Number & Slow labeling codes

sub Group1A {
  
  return if (@_ < 4);

  # Program Item Number

  utter ("  PIN:    ". parsepin($_[3])," PIN:".parsepin($_[3]));

  # Paging (M.2.1.1.2)

  appdata ("Pager", "TNG: ".     bits($_[1], 2, 3));
  appdata ("Pager", "interval: ".bits($_[1], 0, 2));

  # Slow labeling codes
    
  $stn{$pi}{'LA'} = bits($_[2], 15, 1);
  utter ("  LA:     ".($stn{$pi}{'LA'} ? "Program is linked ".(exists($stn{$pi}{'LSN'}) &&
                                      sprintf("to linkage set %Xh ",$stn{$pi}{'LSN'}))."at the moment" :
                                      "Program is not linked at the moment"),
       " LA:".$stn{$pi}{'LA'}.(exists($stn{$pi}{'LSN'}) && sprintf("0x%X",$stn{$pi}{'LSN'})));
   
  my $slc_variant = bits($_[2], 12, 3);

  given ($slc_variant) {

    when (0) {
      appdata ("Pager", "OPC: ".bits($_[2], 8, 4));

      # No PIN, M.3.2.4.3
      if (@_ == 4 && ($_[3] >> 11) == 0) {
        given (bits($_[3], 10, 1)) {
          # Sub type 0
          when (0) {
            appdata ("Pager", "PAC: ".bits($_[3], 4, 6));
            appdata ("Pager", "OPC: ".bits($_[3], 0, 4));
          }
          # Sub type 1
          when (1) {
            given (bits($_[3], 8, 2)) {
              when (0) { appdata ("Pager", "ECC: ".bits($_[3], 0, 6)); }
              when (3) { appdata ("Pager", "CCF: ".bits($_[3], 0, 4)); }
            }
          }
        }
      }

      $stn{$pi}{'ECC'}    = bits($_[2],  0, 8);
      $stn{$pi}{'CC'}     = bits($pi,   12, 4);
      utter (("  ECC:    ".sprintf("%02X", $stn{$pi}{'ECC'}).
        (defined $countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] &&
              " ($countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}])"),
           (" ECC:".sprintf("%02X", $stn{$pi}{'ECC'}).
        (defined $countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] &&
              "[$countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}]]" ))));
    }

    when (1) {
      $stn{$pi}{'tmcid'}       = bits($_[2], 0, 12);
      utter ("  TMC ID: ". sprintf("%xh",$stn{$pi}{'tmcid'}), " TMCID:".sprintf("%xh",$stn{$pi}{'tmcid'}));
    }

    when (2) {
      appdata ("Pager", "OPC: ".bits($_[2], 8, 4));
      appdata ("Pager", "PAC: ".bits($_[2], 0, 6));
      
      # No PIN, M.3.2.4.3
      if (@_ == 4 && ($_[3] >> 11) == 0) {
        given (bits($_[3], 10, 1)) {
          # Sub type 0
          when (0) {
            appdata ("Pager", "PAC: ".bits($_[3], 4, 6));
            appdata ("Pager", "OPC: ".bits($_[3], 0, 4));
          }
          # Sub type 1
          when (1) {
            given (bits($_[3], 8, 2)) {
              when (0) { appdata ("Pager", "ECC: ".bits($_[3], 0, 6)); }
              when (3) { appdata ("Pager", "CCF: ".bits($_[3], 0, 4)); }
            }
          }
        }
      }
    }

    when (3) {
      $stn{$pi}{'lang'}        = bits($_[2], 0, 8);
      utter ("  Lang:   ". sprintf( ($stn{$pi}{'lang'} <= 127 ?
        "0x%X $langname[$stn{$pi}{'lang'}]" : "Unknown language %Xh"), $stn{$pi}{'lang'}),
           " LANG:".sprintf( ($stn{$pi}{'lang'} <= 127 ?
                      "0x%X[$langname[$stn{$pi}{'lang'}]]" : "%Hx[?]"), $stn{$pi}{'lang'}));
    }

    when (6) {
      utter ("  Brodcaster data: ".sprintf("%03x", bits($_[2], 0, 12)),
             " BDATA:".sprintf("%03x", bits($_[2], 0, 12)));
    }

    when (7) {
      $stn{$pi}{'EWS_channel'} = bits($_[2], 0, 12);
      utter ("  EWS channel: ". sprintf("0x%X",$stn{$pi}{'EWS_channel'}),
             " EWSch:". sprintf("0x%X",$stn{$pi}{'EWS_channel'}));
    }

    default {
      say "          SLC variant $slc_variant is not assigned in standard";
    }

  }
}

# 1B: Program Item Number

sub Group1B {
  return if (@_ < 4);
  utter ("  PIN:    ". parsepin($_[3])," PIN:$_[3]");
}
  
# 2A: RadioText (64 characters)

sub Group2A {

  return if (@_ < 3);

  my $text_seg_addr        = bits($_[1], 0, 4) * 4;
  $stn{$pi}{'prev_textAB'} = $stn{$pi}{'textAB'};
  $stn{$pi}{'textAB'}      = bits($_[1], 4, 1);
  my @chr                  = ();

  $chr[0] = bits($_[2], 8, 8);
  $chr[1] = bits($_[2], 0, 8);

  if (@_ == 4) {
    $chr[2] = bits($_[3], 8, 8);
    $chr[3] = bits($_[3], 0, 8);
  }

  # Page 26
  if (($stn{$pi}{'prev_textAB'} // -1) != $stn{$pi}{'textAB'}) {
    if ($stn{$pi}{'denyRTAB'} // FALSE) {
      utter ("          (Ignoring A/B flag change)"," denyRTAB");
    } else {
      utter ("          (A/B flag change; text reset)"," RT_RESET");
      $stn{$pi}{'RTbuf'}  = " " x 64;
      $stn{$pi}{'RTrcvd'} = ();
    }
  }

  set_rt_khars($text_seg_addr,@chr);
}

# 2B: RadioText (32 characters)

sub Group2B {

  return if (@_ < 4);

  my $text_seg_addr        = bits($_[1], 0, 4) * 2;
  $stn{$pi}{'prev_textAB'} = $stn{$pi}{'textAB'};
  $stn{$pi}{'textAB'}      = bits($_[1], 4, 1);
  my @chr                  = (bits($_[3], 8, 8), bits($_[3], 0, 8));

  if (($stn{$pi}{'prev_textAB'} // -1) != $stn{$pi}{'textAB'}) {
    if ($stn{$pi}{'denyRTAB'} // FALSE) {
      utter ("          (Ignoring A/B flag change)"," denyRTAB");
    } else {
      utter ("          (A/B flag change; text reset)"," RT_RESET");
      $stn{$pi}{'RTbuf'}  = " " x 64;
      $stn{$pi}{'RTrcvd'} = ();
    }
  }

  set_rt_khars($text_seg_addr,@chr);

}

# 3A: Application Identification for Open Data

sub Group3A {

  return if (@_ < 4); 

  my $gtype = bits($_[1], 0, 5);
 
  given ($gtype) { 

    when (0) {
      utter ("  ODAapp: ". ($oda_app{$_[3]} // sprintf("0x%04X",$_[3])), " ODAapp:".sprintf("0x%04X",$_[3]));
      utter ("          is not carried in associated group","[not_carried]");
      return;
    }

    when (32) {
      utter ("  ODA:    Temporary data fault (Encoder status)"," ODA:enc_err");
      return;
    }

    when ([0..6, 8, 20, 28, 29, 31]) {
      utter ("  ODA:    (Illegal Application Group Type)"," ODA:err");
      return;
    }

    default {
      $stn{$pi}{'ODAaid'}{$gtype} = $_[3];
      utter ("  ODAgrp: ". bits($_[1], 1, 4) . (bits($_[1], 0, 1) ? "B" : "A"),
           " ODAgrp:". bits($_[1], 1, 4) . (bits($_[1], 0, 1) ? "B" : "A"));
      utter ("  ODAapp: ". ($oda_app{$stn{$pi}{'ODAaid'}{$gtype}} // sprintf("%04Xh",$stn{$pi}{'ODAaid'}{$gtype})),
           " ODAapp:". sprintf("0x%04X",$stn{$pi}{'ODAaid'}{$gtype}));
    }

  }

  given ($stn{$pi}{'ODAaid'}{$gtype}) {

    # Traffic Message Channel
    when ([0xCD46, 0xCD47]) {
      $stn{$pi}{'hasTMC'} = TRUE;
      appdata ("TMC", sprintf("sysmsg %04x",$_[2]));
    }

    # RT+
    when (0x4BD7) {
      $stn{$pi}{'hasRTplus'} = TRUE;
      $stn{$pi}{'rtp_which'} = bits($_[2], 13, 1);
      $stn{$pi}{'CB'}        = bits($_[2], 12, 1);
      $stn{$pi}{'SCB'}       = bits($_[2],  8, 4);
      $stn{$pi}{'templnum'}  = bits($_[2],  0, 8);
      utter ("  RT+ applies to ".($stn{$pi}{'rtp_which'} ? "enhanced RadioText" : "RadioText"), "");
      utter ("  ".($stn{$pi}{'CB'} ? "Using template $stn{$pi}{'templnum'}" : "No template in use"), "");
      if (!$stn{$pi}{'CB'}) {
        utter (sprintf("  Server Control Bits: %Xh", $stn{$pi}{'SCB'}),
               sprintf(" SCB:%Xh", $stn{$pi}{'SCB'}));
      }
    }

    # eRT
    when (0x6552) {
      $stn{$pi}{'haseRT'}     = TRUE;
      $stn{$pi}{'eRTbuf'}     = " " x 64 if (not exists $stn{$pi}{'eRTbuf'});
      $stn{$pi}{'ert_isutf8'} = bits($_[2], 0, 1);
      $stn{$pi}{'ert_txtdir'} = bits($_[2], 1, 1);
      $stn{$pi}{'ert_chrtbl'} = bits($_[2], 2, 4);
    }
    
    # Unimplemented ODA
    default {
      say "  ODAmsg: ". sprintf("%04x",$_[2])            ;
      say "          Unimplemented Open Data Application";
    }
  }
}

# 4A: Clock-time and date
    
sub Group4A {

  return if (@_ < 3);
  
  my $lto;
  my $mjd = (bits($_[1], 0, 2) << 15) | bits($_[2], 1, 15);

  if (@_ == 4) {
    # Local time offset
    $lto =  bits($_[3], 0, 5) / 2;
    $lto = (bits($_[3], 5, 1) ? -$lto : $lto);
    $mjd = int($mjd + $lto / 24);
  }

  my $yr  = int(($mjd - 15078.2) / 365.25);
  my $mo  = int(($mjd - 14956.1 - int($yr * 365.25)) / 30.6001);
  my $dy  = $mjd-14956 - int($yr * 365.25) - int($mo * 30.6001);
  my $k   = ($mo== 14 || $mo == 15);
  $yr += $k + 1900;
  $mo -= 1 + $k * 12;
  #$wd = ($mjd + 2) % 7;

  if (@_ == 4) {
    my $ltom = ($lto - int($lto)) * 60;
    $lto = int($lto);
      
    my $hr = ( ( bits($_[2], 0, 1) << 4) | bits($_[3], 12, 4) + $lto) % 24;
    my $mn = bits($_[3], 6, 6);

    utter ("  CT:     ". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13 && $hr > 0 && $hr < 24 && $mn > 0 && $mn < 60) ?
          sprintf("%04d-%02d-%02dT%02d:%02d%+03d:%02d", $yr, $mo, $dy, $hr, $mn, $lto, $ltom) :
          "Invalid datetime data"),
           " CT:". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13 && $hr > 0 && $hr < 24 && $mn > 0 && $mn < 60) ?
           sprintf("%04d-%02d-%02dT%02d:%02d%+03d:%02d", $yr, $mo, $dy, $hr, $mn, $lto, $ltom) :
           "err"));
  } else {
    utter ("  CT:     ". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13) ?
          sprintf("%04d-%02d-%02d", $yr, $mo, $dy) :
          "Invalid datetime data"),
          " CT:". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13) ?
                    sprintf("%04d-%02d-%02d", $yr, $mo, $dy) :
                              "err"));
  }

}

# 5A: Transparent data channels or ODA

sub Group5A {

  return if (@_ < 4);

  my $addr = bits($_[1], 0, 5);
  say "  TDChan: $addr";
  say sprintf("  TDS:    %02x %02x %02x %02x",
    bits($_[2], 8, 8), bits($_[2], 0, 8), bits($_[3], 8, 8),  bits($_[3], 0, 8));
}

# 5B: Transparent data channels or ODA

sub Group5B {

  return if (@_ < 4);

  my $addr = bits($_[1], 0, 5);
  say "  TDChan: $addr";
  say sprintf("  TDS:    %02x %02x", bits($_[3], 8, 8), bits($_[3], 0, 8));
}


# 6A: In-House Applications or ODA
    
sub Group6A {

  return if (@_ < 4);
  say "  IH:     ". sprintf("%02x %04x %04x", bits($_[1], 0, 5), $_[2], $_[3]);

}

# 6B: In-House Applications or ODA
    
sub Group6B {

  return if (@_ < 4);
  say "  IH:     ". sprintf("%02x %04x", bits($_[1], 0, 5), $_[3]);

}

# 7A: Radio Paging or ODA

sub Group7A {
  
  return if (@_ < 3);
  appdata ("Pager", sprintf("7A: %02x %04x %04x",bits($_[1], 0, 5), $_[2], $_[3]));
}

# 9A: Emergency warning systems or ODA

sub Group9A {

  return if (@_ < 4);
  say "  EWS:    ". sprintf("%02x %04x %04x", bits($_[1], 0, 5), $_[2], $_[3]);

}

# 10A: Program Type Name (PTYN)

sub Group10A {

  if (bits($_[1], 4, 1) != ($stn{$pi}{'PTYNAB'} // -1)) {
    say "         (A/B flag change, text reset)";
    $stn{$pi}{'PTYN'} = " " x 8;
  }

  $stn{$pi}{'PTYNAB'} = bits($_[1], 4, 1);

  if (@_ >= 3) {
    my @chr = ();
    $chr[0]  = bits($_[2], 8, 8);
    $chr[1]  = bits($_[2], 0, 8);

    if (@_ == 4) {
      $chr[2] = bits($_[3], 8, 8);
      $chr[3] = bits($_[3], 0, 8);
    }

    my $segaddr = bits($_[1], 0, 1);

    for my $cnum (0..$#chr) {
      substr($stn{$pi}{'PTYN'}, $segaddr*4 + $cnum, 1) = $charbasic[$chr[$cnum]];
    }
        
    say "  PTYN:   ", substr($stn{$pi}{'PTYN'},0,$segaddr*4).REVERSE.substr($stn{$pi}{'PTYN'},$segaddr*4,scalar(@chr)).
                RESET.substr($stn{$pi}{'PTYN'},$segaddr*4+scalar(@chr));
  }
}

# 13A: Enhanced Radio Paging or ODA

sub Group13A {
 
  return if (@_ < 4);
  appdata ("Pager", sprintf("13A: %02x %04x %04x",bits($_[1], 0, 5), $_[2], $_[3]));

}

# 14A: Enhanced Other Networks (EON) information

sub Group14A {

  return if (@_ < 4);

  $stn{$pi}{'hasEON'}    = TRUE;
  my $eon_pi             = $_[3];
  $stn{$eon_pi}{'TP'}    = bits($_[1], 4, 1);
  my $eon_variant        = bits($_[1], 0, 4);
  utter ("  Other Network"," ON:");
  utter ("    PI:     ".sprintf("%04X",$eon_pi).((exists($stn{$eon_pi}{'chname'})) && " ($stn{$eon_pi}{'chname'})"),
       sprintf("%04X[",$eon_pi));
  utter ("    TP:     $TPtext[$stn{$eon_pi}{'TP'}]","TP:$stn{$eon_pi}{'TP'}");

  given ($eon_variant) {

    when ([0..3]) {
      utter("  ","");
      $stn{$eon_pi}{'PSbuf'} = " " x 8 unless (exists($stn{$eon_pi}{'PSbuf'}));
      set_ps_khars($eon_pi, $eon_variant*2, bits($_[2], 8, 8), bits($_[2], 0, 8));
    }

    when (4) {
      utter ("    AF:     ".parseAF(TRUE, bits($_[2], 8, 8)), " AF:".parseAF(TRUE, bits($_[2], 8, 8)));
      utter ("    AF:     ".parseAF(TRUE, bits($_[2], 0, 8)), " AF:".parseAF(TRUE, bits($_[2], 0, 8)));
    }

    when ([5..8]) {
      utter("    AF:     Tuned frequency ".parseAF(TRUE, bits($_[2], 8, 8))." maps to ".
                                           parseAF(TRUE, bits($_[2], 0, 8)),
            " AF:map:".parseAF(TRUE, bits($_[2], 8, 8))."->".parseAF(TRUE, bits($_[2], 0, 8)));
    }

    when (9) {
      utter ("    AF:     Tuned frequency ".parseAF(TRUE, bits($_[2], 8, 8))." maps to ".
                                            parseAF(FALSE,bits($_[2], 0, 8)),
             " AF:map:".parseAF(TRUE, bits($_[2], 8, 8))."->".parseAF(FALSE,bits($_[2], 0, 8)));
    }

    when (12) {
      $stn{$eon_pi}{'LA'}  = bits($_[2], 15,  1);
      $stn{$eon_pi}{'EG'}  = bits($_[2], 14,  1);
      $stn{$eon_pi}{'ILS'} = bits($_[2], 13,  1);
      $stn{$eon_pi}{'LSN'} = bits($_[2], 1,  12);
      if ($stn{$eon_pi}{'LA'})  {
        utter ("    Link: Program is linked to linkage set ".
               sprintf("%03X",$stn{$eon_pi}{'LSN'}),
               " LSN:".sprintf("%03X", $stn{$eon_pi}{'LSN'}));
      }
      if ($stn{$eon_pi}{'EG'})  {
        utter ("    Link: Program is member of an extended generic set"," Link:EG");
      }
      if ($stn{$eon_pi}{'ILS'}) {
        utter ("    Link: Program is linked internationally", "Link:ILS");
      }
      # TODO: Country codes, pg. 51
    }

    when (13) {
      $stn{$eon_pi}{'PTY'} = bits($_[2], 11, 5);
      $stn{$eon_pi}{'TA'}  = bits($_[2],  0, 1);
      utter (("    PTY:    $stn{$eon_pi}{'PTY'} ".
        (exists $stn{$eon_pi}{'ECC'} && ($countryISO[$stn{$pi}{'ECC'}][$stn{$eon_pi}{'CC'}] // "") =~ /us|ca|mx/ ? 
          $ptynamesUS[$stn{$eon_pi}{'PTY'}] :
          $ptynames[$stn{$eon_pi}{'PTY'}])),
        " PTY:$stn{$eon_pi}{'PTY'}");
      utter ("    TA:     $TAtext[$stn{$eon_pi}{'TP'}][$stn{$eon_pi}{'TA'}]"," TA:$stn{$eon_pi}{'TA'}");
    }

    when (14) {
      utter ("    PIN:    ". parsepin($_[2])," PIN:".parsepin($_[2]));
    }

    when (15) {
      utter ("    Broadcaster data: ".sprintf("%04x", $_[2]), " BDATA:".sprintf("%04x", $_[2]));
    }

    default {
      say "    EON variant $eon_variant is unallocated";
    }

  }
  utter("","]");
}

# 14B: Enhanced Other Networks (EON) information

sub Group14B {

  return if (@_ < 4);

  my $eon_pi          =  $_[3];
  $stn{$eon_pi}{'TP'} = bits($_[1], 4, 1);
  $stn{$eon_pi}{'TA'} = bits($_[1], 3, 1);
  utter ("  Other Network"," ON:");
  utter ("    PI:     ".sprintf("%04X",$eon_pi).((exists($stn{$eon_pi}{'chname'})) && " ($stn{$eon_pi}{'chname'})"),
       sprintf("%04X[",$eon_pi));
  utter ("    TP:     $TPtext[$stn{$eon_pi}{'TP'}]","TP:$stn{$eon_pi}{'TP'}");
  utter ("    TA:     $TAtext[$stn{$eon_pi}{'TP'}][$stn{$eon_pi}{'TA'}]","TA:$stn{$eon_pi}{'TA'}");
}

# 15B: Fast basic tuning and switching information

sub Group15B {
  
  # DI
  my $DI_adr = 3 - bits($_[1], 0, 2);
  my $DI     = bits($_[1], 2, 1);
  parseDI($DI_adr, $DI);

  # TA, M/S
  $stn{$pi}{'TA'} = bits($_[1], 4, 1);
  $stn{$pi}{'MS'} = bits($_[1], 3, 1);
  utter ("  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]", " TA:$stn{$pi}{'TA'}");
  utter ("  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}],      " MS:".qw( S M)[$stn{$pi}{'MS'}]);
  $stn{$pi}{'hasMS'} = TRUE;

}

# Any group used for Open Data

sub ODAGroup {

  my ($gtype, @data) = @_;
  
  return if (@data < 4);

  if (exists $stn{$pi}{'ODAaid'}{$gtype}) {
    given ($stn{$pi}{'ODAaid'}{$gtype}) {

      when ([0xCD46, 0xCD47]) { appdata ("TMC", sprintf("msg %02x %04x %04x",
                                bits($data[1], 0, 5), $data[2], $data[3])); }
      when (0x4BD7)           { parse_RTp(@data); }
      when (0x6552)           { parse_eRT(@data); }
      default                 {
        say sprintf("          Unimplemented ODA %04x: %02x %04x %04x",
                                    $stn{$pi}{'ODAaid'}{$gtype}, bits($data[1], 0, 5), $data[2], $data[3]);
      }

    }
  } else {
    utter ("          Will need group 3A first to identify ODA", "");
  }
}

sub screenReset {

  $stn{$pi}{'RTbuf'} = (" " x 64) if (!exists $stn{$pi}{'RTbuf'});
  $stn{$pi}{'hasRT'} = FALSE      if (!exists $stn{$pi}{'hasRT'});
  $stn{$pi}{'hasMS'} = FALSE      if (!exists $stn{$pi}{'hasMS'});
  $stn{$pi}{'TP'}    = FALSE      if (!exists $stn{$pi}{'TP'});
  $stn{$pi}{'TA'}    = FALSE      if (!exists $stn{$pi}{'TA'});

}

# Change characters in RadioText

sub set_rt_khars {
  (my $lok, my @a) = @_;

  $stn{$pi}{'hasRT'} = TRUE;

  for my $i (0..$#a) {
    given ($a[$i]) {
      when (0x0D) { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = "↵";                }
      when (0x0A) { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = "␊";                }
      default     { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = $charbasic[$a[$i]]; }
    }
    $stn{$pi}{'RTrcvd'}[$lok+$i] = TRUE;
  }

  my $minRTlen = ($stn{$pi}{'RTbuf'} =~ /↵/ ? index($stn{$pi}{'RTbuf'},"↵") + 1 : $stn{$pi}{'presetminRTlen'} // 64);
 
  my $totrc = grep (defined $_, @{$stn{$pi}{'RTrcvd'}}[0..$minRTlen]);
  $stn{$pi}{'hasFullRT'} = ($totrc >= $minRTlen ? TRUE : FALSE);

  my $displayedRT = ($interactive ? substr($stn{$pi}{'RTbuf'},0,$lok).REVERSE.
                      substr($stn{$pi}{'RTbuf'},$lok,scalar(@a)).RESET.
                      substr($stn{$pi}{'RTbuf'},$lok+scalar(@a)) : $stn{$pi}{'RTbuf'});
  utter ("  RT:     ".$displayedRT, " RT:\"".$displayedRT."\"");
  if ($stn{$pi}{'hasFullRT'}) {
    utter ("", " RTcomplete");
  }

  utter ("          ". join("", (map ((defined) ? "^" : " ", @{$stn{$pi}{'RTrcvd'}}[0..63]))),"");
}

# Enhanced RadioText

sub parse_eRT {
  my $addr = bits($_[1], 0, 5);

  if ($stn{$pi}{'ert_chrtbl'} == 0x00 &&
     !$stn{$pi}{'ert_isutf8'}         &&
      $stn{$pi}{'ert_txtdir'} == 0) {
    
    for (0..1) {
      substr($stn{$pi}{'eRTbuf'}, 2*$addr+$_, 1) = decode("UCS-2LE", chr(bits($_[2+$_], 8, 8)).chr(bits($_[2+$_], 0, 8)));
      $stn{$pi}{'eRTrcvd'}[2*$addr+$_]           = TRUE;
    }
  
    say "  eRT:    ". substr($stn{$pi}{'eRTbuf'},0,2*$addr).($interactive ? REVERSE : "").
                      substr($stn{$pi}{'eRTbuf'},2*$addr,2).($interactive ? RESET : "").
                      substr($stn{$pi}{'eRTbuf'},2*$addr+2);
  
    say "          ". join("", (map ((defined) ? "^" : " ", @{$stn{$pi}{'eRTrcvd'}}[0..63])));

  }
}

# Change characters in the Program Service name

sub set_ps_khars {
  my $pspi = $_[0];
  my $lok  = $_[1];
  my @khar = ($_[2], $_[3]);
  my $markup;
    
  if (not exists $stn{$pspi}{'PSbuf'}) {
    $stn{$pspi}{'PSbuf'} = " " x 8
  }

  substr($stn{$pspi}{'PSbuf'}, $lok, 2) = $charbasic[$khar[0]] . $charbasic[$khar[1]];

  # Display PS name when received without gaps

  if (not exists $stn{$pspi}{'prevPSlok'}) {
    $stn{$pspi}{'prevPSlok'} = 0;
  }
  if ($lok != $stn{$pspi}{'prevPSlok'} + 2 || $lok == $stn{$pspi}{'prevPSlok'}) {
    $stn{$pspi}{'PSrcvd'} = ();
  }
  $stn{$pspi}{'PSrcvd'}[$lok/2] = TRUE;
  $stn{$pspi}{'prevPSlok'}      = $lok;
  my $totrc                     = grep (defined, @{$stn{$pspi}{'PSrcvd'}}[0..3]);

  if ($totrc == 4) {
    ($markup = $stn{$pspi}{'PSbuf'}) =~ s/&/&amp;/g;
    $markup =~ s/</&lt;/g;
  }

  my $displayedPS = ($interactive ? substr($stn{$pspi}{'PSbuf'},0,$lok).REVERSE.
                      substr($stn{$pspi}{'PSbuf'},$lok,2).RESET.
                      substr($stn{$pspi}{'PSbuf'},$lok+2) : $stn{$pspi}{'PSbuf'});
  utter ("  PS:     ".$displayedPS, " PS:\"".$displayedPS."\"");

}

# RadioText+

sub parse_RTp {

  my @ctype;
  my @start;
  my @len;

  # P.5.2
  my $itog  = bits($_[1], 4, 1);
  my $irun  = bits($_[1], 3, 1);
  $ctype[0] = (bits($_[1], 0, 3) << 3) + bits($_[2], 13, 3);
  $ctype[1] = (bits($_[2], 0, 1) << 5) + bits($_[3], 11, 5);
  $start[0] = bits($_[2], 7, 6);
  $start[1] = bits($_[3], 5, 6);
  $len[0]   = bits($_[2], 1, 6);
  $len[1]   = bits($_[3], 0, 5);

  say "  RadioText+: ";

  if ($irun) {
    say "    Item running";
    if ($stn{$pi}{'rtp_which'} == 0) {
      for my $tag (0..1) {
        my $totrc = grep (defined $_, @{$stn{$pi}{'RTrcvd'}}[$start[$tag]..($start[$tag]+$len[$tag]-1)]);
        if ($totrc == $len[$tag]) {
          say "    Tag $rtpclass[$ctype[$tag]]: ".substr($stn{$pi}{'RTbuf'}, $start[$tag], $len[$tag]);
        }
      }
    } else {
      # (eRT)
    }
  } else {
    say "    No item running";
  }

}

# Program Item Number

sub parsepin {
  my $d   = bits($_[0], 11, 5);
  return ($d ? sprintf("%02d@%02d:%02d",$d, bits($_[0], 6, 5), bits($_[0], 0, 6)) : "None");
}

# Decoder Identification

sub parseDI {
  given ($_[0]) {
    when (0) {
      utter ("  DI:     ". qw( Mono Stereo )[$_[1]], " DI:".qw( Mono Stereo )[$_[1]]);
    }
    when (1) {
      if ($_[1]) {
        utter ("  DI:     Artificial head", " DI:ArtiHd");
      }
    }
    when (2) {
      if ($_[1]) {
        utter ("  DI:     Compressed", " DI:Cmprsd");
      }
    }
    when (3) {
      utter ("  DI:     ". qw( Static Dynamic)[$_[1]] ." PTY", " DI:".qw( StaPTY DynPTY )[$_[1]]);
    }
  }
}

# Alternate Frequencies

sub parseAF {
  my $fm  = shift;
  my $num = shift;
  my $af;
  if ($fm) {
    given ($num) {
      when ([1..204])   { $af = sprintf("%0.1fMHz", (87.5 + $num/10) ); }
      when (205)        { $af = "(filler)"; }
      when (224)        { $af = "\"No AF exists\""; }
      when ([225..249]) { $af = "\"".($num == 225 ? "1 AF follows" : ($num-224)." AFs follow")."\""; }
      when (250)        { $af = "\"AM/LF freq follows\""; }
      default           { $af = "(error:$num)"; }
    }
  } else {
    given ($num) {
      when ([1..15])    { $af = sprintf("%dkHz", 144 + $num * 9); }
      when ([16..135])  { $af = sprintf("%dkHz", 522 + ($num-15) * 9); }
      default           { $af = "N/A"; }
    }
  }
  return $af;
}



sub initdata {

  # Program Type names
  @ptynames   = ("No PTY",           "News",             "Current Affairs",  "Information",
                 "Sport",            "Education",        "Drama",            "Cultures",
                 "Science",          "Varied Speech",    "Pop Music",        "Rock Music",
                 "Easy Listening",   "Light Classics M", "Serious Classics", "Other Music",
                 "Weather & Metr",   "Finance",          "Children's Progs", "Social Affairs",
                 "Religion",         "Phone In",         "Travel & Touring", "Leisure & Hobby",
                 "Jazz Music",       "Country Music",    "National Music",   "Oldies Music",
                 "Folk Music",       "Documentary",      "Alarm Test",       "Alarm - Alarm !");

  # PTY names for the US (RBDS)
  @ptynamesUS = ("No PTY",           "News",             "Information",      "Sports",
                 "Talk",             "Rock",             "Classic Rock",     "Adult Hits",
                 "Soft Rock",        "Top 40",           "Country",          "Oldies",
                 "Soft",             "Nostalgia",        "Jazz",             "Classical",
                 "Rhythm and Blues", "Soft R & B",       "Foreign Language", "Religious Music",
                 "Religious Talk",   "Personality",      "Public",           "College",
                 "",                 "",                 "",                 "",
                 "",                 "Weather",          "Emergency Test",   "ALERT! ALERT!");

  #@ptynamesFI = ("",                 "Uutiset",          "Ajankohtaista",    "Tiedotuksia",
               #  "Urheilu",          "Opetus",           "Kuunnelma",        "Kulttuuri",
               #  "Tiede",            "Puheviihde",       "Pop",              "Rock",
               #  "Kevyt musiikki",   "Kevyt klassinen",  "Klassinen",        "Muu musiikki",
               #  "Säätiedotus",      "Talousohjelma",    "Lastenohjelma",    "Yhteiskunta",
               #  "Uskonto",          "Yleisökontakti",   "Matkailu",         "Vapaa-aika",
               #  "Jazz",             "Country",          "Kotim. musiikki",  "Oldies",
               #  "Kansanmusiikki",   "Dokumentti",       "HÄLYTYS TESTI",    "HÄLYTYS");
  
  # Basic LCD character set
  @charbasic = split(//,
               '                ' . '                ' . ' !"#¤%&\'()*+,-./'. '0123456789:;<=>?' .
               '@ABCDEFGHIJKLMNO' . 'PQRSTUVWXYZ[\\]―_'. '‖abcdefghijklmno' . 'pqrstuvwxyz{|}¯ ' .
               'áàéèíìóòúùÑÇŞβ¡Ĳ' . 'âäêëîïôöûüñçşǧıĳ' . 'ªα©‰Ǧěňőπ€£$←↑→↓' . 'º¹²³±İńűµ¿÷°¼½¾§' .
               'ÁÀÉÈÍÌÓÒÚÙŘČŠŽÐĿ' . 'ÂÄÊËÎÏÔÖÛÜřčšžđŀ' . 'ÃÅÆŒŷÝÕØÞŊŔĆŚŹŦð' . 'ãåæœŵýõøþŋŕćśźŧ ');
  
  # Meanings of combinations of TP+TA
  @TPtext = (  "Does not carry traffic announcements", "Program carries traffic announcements" );
  @TAtext = ( ["No EON with traffic announcements",    "EON specifies another program with traffic announcements"],
              ["No traffic announcement at present",   "A traffic announcement is currently being broadcast"]);
  
  # Language names
  @langname = (
     "Unknown",      "Albanian",      "Breton",     "Catalan",    "Croatian",    "Welsh",     "Czech",      "Danish",
     "German",       "English",       "Spanish",    "Esperanto",  "Estonian",    "Basque",    "Faroese",    "French",
     "Frisian",      "Irish",         "Gaelic",     "Galician",   "Icelandic",   "Italian",   "Lappish",    "Latin",
     "Latvian",      "Luxembourgian", "Lithuanian", "Hungarian",  "Maltese",     "Dutch",     "Norwegian",  "Occitan",
     "Polish",       "Portuguese",    "Romanian",   "Romansh",    "Serbian",     "Slovak",    "Slovene",    "Finnish",
     "Swedish",      "Turkish",       "Flemish",    "Walloon",    "",            "",          "",           "",
     "",             "",              "",           "",           "",            "",          "",           "",
     "",             "",              "",           "",           "",            "",          "",           "",
     "Background",   "",              "",           "",           "",            "Zulu",      "Vietnamese", "Uzbek",
     "Urdu",         "Ukrainian",     "Thai",       "Telugu",     "Tatar",       "Tamil",     "Tadzhik",    "Swahili",
     "Sranan Tongo", "Somali",        "Sinhalese",  "Shona",      "Serbo-Croat", "Ruthenian", "Russian",    "Quechua",
     "Pushtu",       "Punjabi",       "Persian",    "Papamiento", "Oriya",       "Nepali",    "Ndebele",    "Marathi",
     "Moldovian",    "Malaysian",     "Malagasay",  "Macedonian", "Laotian",     "Korean",    "Khmer",      "Kazakh",
     "Kannada",      "Japanese",      "Indonesian", "Hindi",      "Hebrew",      "Hausa",     "Gurani",     "Gujurati",
     "Greek",        "Georgian",      "Fulani",     "Dari",       "Churash",     "Chinese",   "Burmese",    "Bulgarian",
     "Bengali",      "Belorussian",   "Bambora",    "Azerbaijan", "Assamese",    "Armenian",  "Arabic",     "Amharic" );
  
  # Open Data Applications
  %oda_app = (
     0x0000 => "Normal features specified in Standard",            0x0093 => "Cross referencing DAB within RDS",
     0x0BCB => "Leisure & Practical Info for Drivers",             0x0C24 => "ELECTRABEL-DSM 7",
     0x0CC1 => "Wireless Playground broadcast control signal",     0x0D45 => "RDS-TMC: ALERT-C / EN ISO 14819-1",
     0x0D8B => "ELECTRABEL-DSM 18",                                0x0E2C => "ELECTRABEL-DSM 3",
     0x0E31 => "ELECTRABEL-DSM 13",                                0x0F87 => "ELECTRABEL-DSM 2",
     0x125F => "I-FM-RDS for fixed and mobile devices",            0x1BDA => "ELECTRABEL-DSM 1",
     0x1C5E => "ELECTRABEL-DSM 20",                                0x1C68 => "ITIS In-vehicle data base",
     0x1CB1 => "ELECTRABEL-DSM 10",                                0x1D47 => "ELECTRABEL-DSM 4",
     0x1DC2 => "CITIBUS 4",                                        0x1DC5 => "Encrypted TTI using ALERT-Plus",
     0x1E8F => "ELECTRABEL-DSM 17",                                0x4AA1 => "RASANT",
     0x4AB7 => "ELECTRABEL-DSM 9",                                 0x4BA2 => "ELECTRABEL-DSM 5",
     0x4BD7 => "RadioText+ (RT+)",                                 0x4C59 => "CITIBUS 2",
     0x4D87 => "Radio Commerce System (RCS)",                      0x4D95 => "ELECTRABEL-DSM 16",
     0x4D9A => "ELECTRABEL-DSM 11",                                0x5757 => "Personal weather station",
     0x6552 => "Enhanced RadioText (eRT)",                         0x7373 => "Enhanced early warning system",
     0xC350 => "NRSC Song Title and Artist",                       0xC3A1 => "Personal Radio Service",
     0xC3B0 => "iTunes Tagging",                                   0xC3C3 => "NAVTEQ Traffic Plus",
     0xC4D4 => "eEAS",                                             0xC549 => "Smart Grid Broadcast Channel",
     0xC563 => "ID Logic",                                         0xC737 => "Utility Message Channel (UMC)",
     0xCB73 => "CITIBUS 1",                                        0xCB97 => "ELECTRABEL-DSM 14",
     0xCC21 => "CITIBUS 3",                                        0xCD46 => "RDS-TMC: ALERT-C / EN ISO 14819 parts 1, 2, 3, 6",
     0xCD47 => "RDS-TMC: ALERT-C / EN ISO 14819 parts 1, 2, 3, 6", 0xCD9E => "ELECTRABEL-DSM 8",
     0xCE6B => "Encrypted TTI using ALERT-Plus",                   0xE123 => "APS Gateway",
     0xE1C1 => "Action code",                                      0xE319 => "ELECTRABEL-DSM 12",
     0xE411 => "Beacon downlink",                                  0xE440 => "ELECTRABEL-DSM 15",
     0xE4A6 => "ELECTRABEL-DSM 19",                                0xE5D7 => "ELECTRABEL-DSM 6",
     0xE911 => "EAS open protocol");
  
  # Country codes: @countryISO[ ECC ][ CC ] = ISO 3166-1 alpha 2
  @{$countryISO[0xA0]} = map { /__/ ? undef : $_ } qw( __ us us us us us us us us us us us __ us us __ );
  @{$countryISO[0xA1]} = map { /__/ ? undef : $_ } qw( __ __ __ __ __ __ __ __ __ __ __ ca ca ca ca gl );
  @{$countryISO[0xA2]} = map { /__/ ? undef : $_ } qw( __ ai ag ec fk bb bz ky cr cu ar br bm an gp bs );
  @{$countryISO[0xA3]} = map { /__/ ? undef : $_ } qw( __ bo co jm mq gf py ni __ pa dm do cl gd tc gy );
  @{$countryISO[0xA4]} = map { /__/ ? undef : $_ } qw( __ gt hn aw __ ms tt pe sr uy kn lc sv ht ve __ );
  @{$countryISO[0xA5]} = map { /__/ ? undef : $_ } qw( __ __ __ __ __ __ __ __ __ __ __ mx vc mx mx mx );
  @{$countryISO[0xA6]} = map { /__/ ? undef : $_ } qw( __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ pm );
  
  @{$countryISO[0xD0]} = map { /__/ ? undef : $_ } qw( __ cm cf dj mg ml ao gq ga gn za bf cg tg bj mw );
  @{$countryISO[0xD1]} = map { /__/ ? undef : $_ } qw( __ na lr gh mr st cv sn gm bi __ bw km tz et bg );
  @{$countryISO[0xD2]} = map { /__/ ? undef : $_ } qw( __ sl zw mz ug sz ke so ne td gw zr ci tz zm __ );
  @{$countryISO[0xD3]} = map { /__/ ? undef : $_ } qw( __ __ __ eh __ rw ls __ sc __ mu __ sd __ __ __ );
  
  @{$countryISO[0xE0]} = map { /__/ ? undef : $_ } qw( __ de dz ad il it be ru ps al at hu mt de __ eg );
  @{$countryISO[0xE1]} = map { /__/ ? undef : $_ } qw( __ gr cy sm ch jo fi lu bg dk gi iq gb ly ro fr );
  @{$countryISO[0xE2]} = map { /__/ ? undef : $_ } qw( __ ma cz pl va sk sy tn __ li is mc lt yu es no );
  @{$countryISO[0xE3]} = map { /__/ ? undef : $_ } qw( __ ie ie tr mk tj __ __ nl lv lb az hr kz se by );
  @{$countryISO[0xE4]} = map { /__/ ? undef : $_ } qw( __ md ee kg __ __ ua __ pt si am uz ge __ tm ba );
  
  @{$countryISO[0xF0]} = map { /__/ ? undef : $_ } qw( __ au au au au au au au au sa af mm cn kp bh my );
  @{$countryISO[0xF1]} = map { /__/ ? undef : $_ } qw( __ ki bt bd pk fj om nr ir nz sb bn lk tw kr hk );
  @{$countryISO[0xF2]} = map { /__/ ? undef : $_ } qw( __ kw qa kh ws in mo vn ph jp sg mv id ae np vu );
  @{$countryISO[0xF3]} = map { /__/ ? undef : $_ } qw( __ la th to __ __ __ __ __ pg __ ye __ __ fm mn );

  # RadioText+ classes
  @rtpclass = ("dummy_class",          "item.title",                "item.album",           "item.tracknumber",
               "item.artist",          "item.composition",          "item.movement",        "item.conductor",
               "item.composer",        "item.band",                 "item.comment",         "item.genre",
               "info.news",            "info.news.local",           "info.stockmarket",     "info.sport",
               "info.lottery",         "info.horoscope",            "info.daily_diversion", "info.health",
               "info.event",           "info.scene",                "info.cinema",          "info.tv",
               "info.date_time",       "info.weather",              "info.traffic",         "info.alarm",
               "info.advertisement",   "info.url",                  "info.other",           "stationname.short",
               "stationname.long",     "programme.now",             "programme.next",       "programme.part",
               "programme.host",       "programme.editorial_staff", "programme.frequency",  "programme.homepage",
               "programme.subchannel", "phone.hotline",             "phone.studio",         "phone.other",
               "sms.studio",           "sms.other",                 "email.hotline",        "email.studio",
               "email.other",          "mms.other",                 "chat",                 "chat.centre",
               "vote.question",        "vote.centre",               "",                     "",
               "",                     "",                          "",                     "place",
               "appointment",          "identifier",                "purchase",             "get_data");
  
  # Group type descriptions
  @groupname = (
   "Basic tuning and switching information",      "Basic tuning and switching information",
   "Program Item Number and slow labeling codes", "Program Item Number",
   "RadioText",                                   "RadioText",
   "Applications Identification for Open Data",   "Open Data Applications",
   "Clock-time and date",                         "Open Data Applications",
   "Transparent Data Channels or ODA",            "Transparent Data Channels or ODA",
   "In House applications or ODA",                "In House applications or ODA",
   "Radio Paging or ODA",                         "Open Data Applications",
   "Traffic Message Channel or ODA",              "Open Data Applications",
   "Emergency Warning System or ODA",             "Open Data Applications",
   "Program Type Name",                           "Open Data Applications",
   "Open Data Applications",                      "Open Data Applications",
   "Open Data Applications",                      "Open Data Applications",
   "Enhanced Radio Paging or ODA",                "Open Data Applications",
   "Enhanced Other Networks information",         "Enhanced Other Networks information",
   "Undefined",                                   "Fast switching information");

}


# Extract len bits from int, starting at nth bit from the right
# bits (int, n, len)
sub bits {
  return (($_[0] >> $_[1]) & (2**$_[2] - 1));
}

sub appdata {
  my ($appname, $data) = @_;
  if (exists $options{t}) {
    my $stamp = strftime("%Y-%m-%dT%H:%M:%S%z ", localtime);
    print $stamp;
  }
  say "[app] $appname $data";
}

sub utter {
  my ($long, $short) = @_;
  if ($verbosity == 0) {
    if ($short =~ /\n/) {
      print "$linebuf$short";
      $linebuf = "";
    } else {
      $linebuf .= $short;
    }
  } elsif ($verbosity == 1) {
    print $long."\n" if ($long ne "");
  }
}

sub log2 {
  return (log($_[0])/log(2));
}
