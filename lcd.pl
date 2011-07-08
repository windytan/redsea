#!/usr/bin/perl

# lcd.pl -- part of redsea RDS decoder (c) OH2-250
#
# decodes session & presentation layers & displays GUI
#
# Page numbers refer to IEC 62106, Edition 2
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

$| ++;

use 5.010;
use warnings;
use utf8;

use IO::Select;
my  $input = IO::Select->new(\*STDIN);

use Gtk2    qw( -init );
use Encode 'decode';
use open   ':utf8';

binmode(STDOUT, ":utf8");

# Booleans
use constant FALSE => 0;
use constant TRUE  => 1;

our $dbg   = TRUE;
our $theme = "green3";

# Some terminal control chars
use constant   RESET => "\x1B[0m";
use constant REVERSE => "\x1B[7m";

# Some bit masks
use constant  _1BIT => 0x0001; use constant  _2BIT => 0x0003;
use constant  _3BIT => 0x0007; use constant  _4BIT => 0x000F;
use constant  _5BIT => 0x001F; use constant  _6BIT => 0x003F;
use constant  _8BIT => 0x00FF; use constant _11BIT => 0x07FF;
use constant _12BIT => 0x0FFF; use constant _15BIT => 0x7FFF;
use constant _16BIT => 0xFFFF;

&initdata;
&initgui;

Glib::Timeout->add(300, \&update_displays);

Gtk2->main;

exit(0);


# 3 times a second

sub update_displays {

  if (++$upd == 10) {
    $qty   = int(($bytes // 0) / 1187.5 / 3 * 5 + .5);
    $qty   = 5 if ($qty > 5);
    $bytes = 0;
    $upd   = 0;
    &updateStatusRow if (defined $pi);
  }

  # Read from data-link layer

  while ($input->can_read(0) && read(STDIN, $dlen, 1)) {

    given (ord($dlen)) {

      when ([1..4]) {
        read(STDIN, $dta, 2*ord($dlen));
        decodegroup(unpack("S*", $dta));
      }

      when (0) {
        $insync = FALSE;
        &updateStatusRow if (defined $pi);
      }

      when (255) {
        $insync = TRUE;
      }

    }

  }

  return TRUE;
}

sub decodegroup {

  return if ($_[0] == 0);
  my ($gtype, $fullgtype);

  $bytes  += @_ * 26;
  $ednewpi = ($newpi // 0);
  $newpi   = $_[0];
  
  if (@_ >= 2) {
    $gtype      =  ($_[1] >> 11) & _5BIT;
    $fullgtype  = (($_[1] >> 12) & _4BIT) . ( (($_[1] >> 11) & _1BIT) ? "B" : "A" );
    say (@_ == 4 ? "Group $fullgtype: $groupname[$gtype]" : "(partial group $fullgtype, ".scalar(@_)." blocks)") if ($dbg);
  } else {
    say "(PI only)" if ($dbg);
  }
  
  say "  PI:     ".sprintf("%04X",$newpi) .((exists($stn{$newpi}{'chname'})) ? " ".$stn{$newpi}{'chname'} : "") if ($dbg);

  # PI is repeated -> confirmed
  if ($newpi == $ednewpi) {

    # PI has changed from last confirmed
    if ($newpi != ($pi // 0)) {
      $pi = $newpi;
      &screenReset();
      $PIlabel->set_markup("<span font='mono 12px' foreground='$themef{fg}'>".sprintf("%04X",$pi)."</span>");
      if (exists $stn{$pi}{'presetPSbuf'}) {
        ($stn{$pi}{'PSmarkup'} = $stn{$pi}{'presetPSbuf'}) =~ s/&/&amp;/g;
        $stn{$pi}{'PSmarkup'}  =~ s/</&lt;/g;
        $PSlabel->set_markup("<span font='Mono Bold 25px' foreground='$themef{fg}'>$stn{$pi}{'PSmarkup'}</span>");
      }
    }

  } elsif ($newpi != ($pi // 0)) {
    say "          (repeat will confirm PI change)\n" if ($dbg);
    return;
  }

  # Nothing more to be done for PI only
  if (@_ == 1) {
    print "\n" if ($dbg);
    return;
  }

  # Traffic Program (TP)
  $stn{$pi}{'TP'} = ($_[1] >> 10) & _1BIT;
  say "  TP:     $TPtext[$stn{$pi}{'TP'}]" if ($dbg);
  &updateStatusRow();
	
  # Program Type (PTY)
  $stn{$pi}{'PTY'} = ($_[1] >> 5) & _5BIT;

  if (exists $stn{$pi}{'ECC'} && ($countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] // "") =~ /us|ca|mx/) {
    $stn{$pi}{'PTYmarkup'} = $ptynamesUS[$stn{$pi}{'PTY'}];
    say "  PTY:    ", sprintf("%02d",$stn{$pi}{'PTY'})." $ptynamesUS[$stn{$pi}{'PTY'}]" if ($dbg);
  } else {
    $stn{$pi}{'PTYmarkup'} = $ptynames[$stn{$pi}{'PTY'}];
    say "  PTY:    ", sprintf("%02d",$stn{$pi}{'PTY'})." $ptynames[$stn{$pi}{'PTY'}]"   if ($dbg);
  }
  $stn{$pi}{'PTYmarkup'} =~ s/&/&amp;/g;

  $PTYlabel->set_markup("<span font='Mono 12px' foreground='$themef{fg}'>$stn{$pi}{'PTYmarkup'}</span>");

  # Data specific to the group type

  given ($gtype) {
    when (0)  { &Group0A (@_); }
    when (1)  { &Group0B (@_); }
    when (2)  { &Group1A (@_); }
    when (3)  { &Group1B (@_); }
    when (4)  { &Group2A (@_); }
    when (5)  { &Group2B (@_); }
    when (6)  { exists ($stn{$pi}{'ODAaid'}{6})  ? &ODAGroup(6, @_)  : &Group3A (@_); }
    when (8)  { &Group4A (@_); }
    when (10) { exists ($stn{$pi}{'ODAaid'}{10}) ? &ODAGroup(10, @_) : &Group5A (@_); }
    when (11) { exists ($stn{$pi}{'ODAaid'}{11}) ? &ODAGroup(11, @_) : &Group5B (@_); }
    when (12) { exists ($stn{$pi}{'ODAaid'}{12}) ? &ODAGroup(12, @_) : &Group6A (@_); }
    when (13) { exists ($stn{$pi}{'ODAaid'}{13}) ? &ODAGroup(13, @_) : &Group6B (@_); }
    when (14) { exists ($stn{$pi}{'ODAaid'}{14}) ? &ODAGroup(14, @_) : &Group7A (@_); }
    when (18) { exists ($stn{$pi}{'ODAaid'}{18}) ? &ODAGroup(18, @_) : &Group9A (@_); }
    when (20) { &Group10A(@_); }
    when (26) { exists ($stn{$pi}{'ODAaid'}{26}) ? &ODAGroup(26, @_) : &Group13A(@_); }
    when (28) { &Group14A(@_); }
    when (29) { &Group14B(@_); }
    when (31) { &Group15B(@_); }

    default   { &ODAGroup($gtype, @_); }
  }
 
  print("\n") if ($dbg);

}

# 0A: Basic tuning and switching information

sub Group0A {

  # DI
  my $DI_adr = 3 - ($_[1]   & _2BIT);
  my $DI     = ($_[1] >> 2) & _1BIT;
  &parseDI($DI_adr, $DI);

  # TA, M/S
  $stn{$pi}{'TA'} = ($_[1] >> 4) & _1BIT;
  $stn{$pi}{'MS'} = ($_[1] >> 3) & _1BIT;
  say "  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]"  if ($dbg);
  say "  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}] if ($dbg);

  $stn{$pi}{'hasMS'} = TRUE;
  &updateStatusRow;

  if (@_ >= 3) {
    # AF
    my @af;
    for (0..1) {
      $af[$_] = &parseAF(TRUE, $_[2] >> (8-$_*8) & _8BIT);
      say "  AF:     $af[$_]" if ($dbg);
    }
    if ($af[0] =~ /follow/ && $af[1] =~ /Hz/) {
      ($stn{$pi}{'freq'} = $af[1]) =~ s/ [kM]Hz//;
      $Freqlabel->set_markup("<span font='mono 12px' foreground='$themef{fg}'>$stn{$pi}{'freq'}</span>");
    }
  }

  if (@_ == 4) {
      
    # Program Service Name (PS)

    if ($stn{$pi}{'denyPS'}) {
      say "          (Ignoring changes to PS)" if ($dbg);
    } else {
      &set_ps_khars($pi, ($_[1] & _2BIT) * 2, ($_[3] >> 8) & _8BIT, ($_[3] >> 0) & _8BIT);
    }
  }
}

# 0B: Basic tuning and switching information

sub Group0B {
  
  # Decoder Identification
  my $DI_adr = 3 - ($_[1]   & _2BIT);
  my $DI     = ($_[1] >> 2) & _1BIT;
  &parseDI($DI_adr, $DI);

  # Traffic Announcements, Music/Speech
  $stn{$pi}{'TA'} = ($_[1] >> 4) & _1BIT;
  $stn{$pi}{'MS'} = ($_[1] >> 3) & _1BIT;
  say "  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]"  if ($dbg);
  say "  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}] if ($dbg);

  $stn{$pi}{'hasMS'} = TRUE;
  &updateStatusRow;

  if (@_ == 4) {
      
    # Program Service name

    if ($stn{$pi}{'denyPS'}) {
      say "          (Ignoring changes to PS)" if ($dbg);
    } else {
      &set_ps_khars($pi, ($_[1] & _2BIT) * 2, ($_[3] >> 8) & _8BIT, ($_[3] >> 0) & _8BIT);
    }
  }

}
  
# 1A: Program Item Number & Slow labeling codes

sub Group1A {
  
  return if (@_ < 4);

  # Program Item Number

  say "  PIN:    ". &parsepin($_[3]) if ($dbg);

  # Paging (M.2.1.1.2)
	
  say "  ══╡ Pager TNG: ".     (($_[1] >> 2) & _3BIT);
  say "  ══╡ Pager interval: ".(($_[1] >> 0) & _2BIT);

  # Slow labeling codes
    
  $stn{$pi}{'LA'} = ($_[2] >> 15) & _1BIT;
  say "  LA:     ".($stn{$pi}{'LA'} ? "Program is linked ".(exists($stn{$pi}{'LSN'}) &&
                                      sprintf("to linkage set %Xh ",$stn{$pi}{'LSN'}))."at the moment" :
                                      "Program is not linked at the moment") if ($dbg);
   
  my $slc_variant = ($_[2] >> 12) & _3BIT;

  given ($slc_variant) {

    when (0) {
      say "  ══╡ Pager OPC: ".(($_[2] >> 8)  & _4BIT);

      # No PIN, M.3.2.4.3
      if (@_ == 4 && ($_[3] >> 11) == 0) {
        given (($_[3] >> 10) & _1BIT) {
          # Sub type 0
          when (0) {
            say "  ══╡ Pager PAC: ".(($_[3] >> 4)  & _6BIT);
            say "  ══╡ Pager OPC: ".(($_[3] >> 0)  & _4BIT);
          }
          # Sub type 1
          when (1) {
            given (($_[3] >> 8) & _2BIT) {
              when (0) { say "  ══╡ Pager ECC: ".(($_[3] >> 0)  & _6BIT); }
              when (3) { say "  ══╡ Pager CCF: ".(($_[3] >> 0)  & _4BIT); }
            }
          }
        }
      }

      $stn{$pi}{'ECC'}    = ($_[2] >> 0)  & _8BIT;
      $stn{$pi}{'CC'}     = ($pi   >> 12) & _4BIT;
      $ECClabel->set_markup("<span font='Mono 11px' foreground='$themef{fg}'>".($countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] // "  ")."</span>");
      say "  ECC:    ".sprintf("%02X", $stn{$pi}{'ECC'}).
        (defined $countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}] && " ($countryISO[$stn{$pi}{'ECC'}][$stn{$pi}{'CC'}])") if ($dbg);
    }

    when (1) {
      $stn{$pi}{'tmcid'}       = ($_[2] >> 0) & _12BIT;
      say "  TMC ID: ". sprintf("%xh",$stn{$pi}{'tmcid'}) if ($dbg);
    }

    when (2) {
      say "  ══╡ Pager OPC: ".(($_[2] >> 8)  & _4BIT);
      say "  ══╡ Pager PAC: ".(($_[2] >> 0)  & _6BIT);
      
      # No PIN, M.3.2.4.3
      if (@_ == 4 && ($_[3] >> 11) == 0) {
        given (($_[3] >> 10) & _1BIT) {
          # Sub type 0
          when (0) {
            say "  ══╡ Pager PAC: ".(($_[3] >> 4)  & _6BIT);
            say "  ══╡ Pager OPC: ".(($_[3] >> 0)  & _4BIT);
          }
          # Sub type 1
          when (1) {
            given (($_[3] >> 8) & _2BIT) {
              when (0) { say "  ══╡ Pager ECC: ".(($_[3] >> 0)  & _6BIT); }
              when (3) { say "  ══╡ Pager CCF: ".(($_[3] >> 0)  & _4BIT); }
            }
          }
        }
      }
    }

    when (3) {
      $stn{$pi}{'lang'}        = ($_[2] >> 0) & _8BIT;
      say "  Lang:   ". sprintf( ($stn{$pi}{'lang'} <= 127 ?
        "%Xh $langname[$stn{$pi}{'lang'}]" : "Unknown language %Xh"), $stn{$pi}{'lang'}) if ($dbg);
    }

    when (6) {
      say "  Brodcaster data: ".sprintf("%03x", $_[2] & _12BIT);
    }

    when (7) {
      $stn{$pi}{'EWS_channel'} = ($_[2] >> 0) & _12BIT;
      say "  EWS ch: ". sprintf("%Xh",$stn{$pi}{'EWS_channel'}) if ($dbg);
    }

    default {
      say "          SLC variant $slc_variant is not assigned in standard" if ($dbg);
    }

  }
}

# 1B: Program Item Number

sub Group1B {

  return if (@_ < 4);
  say "  PIN:    ". &parsepin($_[3]) if ($dbg);
}
  
# 2A: RadioText (64 characters)

sub Group2A {

  return if (@_ < 3);

  my $text_seg_addr        = (($_[1] >> 0) & _4BIT) * 4;
  $stn{$pi}{'prev_textAB'} = $stn{$pi}{'textAB'};
  $stn{$pi}{'textAB'}      = ($_[1] >> 4) & _1BIT;
  my @chr                  = ();

  $chr[0] = ($_[2] >> 8) & _8BIT;
  $chr[1] = ($_[2] >> 0) & _8BIT;

  if (@_ == 4) {
    $chr[2] = ($_[3] >> 8) & _8BIT;
    $chr[3] = ($_[3] >> 0) & _8BIT;
  }

  # Page 26
  if (($stn{$pi}{'prev_textAB'} // -1) != $stn{$pi}{'textAB'}) {
    if ($stn{$pi}{'denyRTAB'} // FALSE) {
      say "          (Ignoring A/B flag change)"    if ($dbg);
    } else {
      say "          (A/B flag change; text reset)" if ($dbg);
      $stn{$pi}{'RTbuf'}  = " " x 64;
      $stn{$pi}{'RTrcvd'} = ();
    }
  }

  &set_rt_khars($text_seg_addr,@chr);
}

# 2B: RadioText (32 characters)

sub Group2B {

  return if (@_ < 4);

  my $text_seg_addr        = (($_[1] >> 0) & _4BIT) * 2;
  $stn{$pi}{'prev_textAB'} = $stn{$pi}{'textAB'};
  $stn{$pi}{'textAB'}      =  ($_[1] >> 4) & _1BIT;
  my @chr                  = (($_[3] >> 8) & _8BIT, ($_[3] >> 0) & _8BIT);

  if (($stn{$pi}{'prev_textAB'} // -1) != $stn{$pi}{'textAB'}) {
    if ($stn{$pi}{'denyRTAB'} // FALSE) {
      say "          (Ignoring A/B flag change)" if ($dbg);
    } else {
      say "          (A/B flag change; text reset)" if ($dbg);
      $stn{$pi}{'RTbuf'}  = " " x 64;
      $stn{$pi}{'RTrcvd'} = ();
    }
  }

  &set_rt_khars($text_seg_addr,@chr);

}

# 3A: Application Identification for Open Data

sub Group3A {

  return if (@_ < 4); 

  my $gtype = ($_[1] & _5BIT);
 
  given ($gtype) { 

    when (0) {
      say "  ODAapp: ". ($oda_app{$_[3]} // sprintf("%04Xh",$_[3])) if ($dbg);
      say "          is not carried in associated group"            if ($dbg);
      return;
    }

    when (32) {
      say "  ODA:    Temporary data fault (Encoder status)"         if ($dbg);
      return;
    }

    when ([0..6, 8, 20, 28, 29, 31]) {
      say "  ODA:    (Illegal Application Group Type)"              if ($dbg);
      return;
    }

    default {
      $stn{$pi}{'ODAaid'}{$gtype} = $_[3];
      say "  ODAgrp: ". (($_[1] >> 1) & _4BIT). ((($_[1] >> 0) & _1BIT) ? "B" : "A") if ($dbg);
      say "  ODAapp: ". ($oda_app{$stn{$pi}{'ODAaid'}{$gtype}} // sprintf("%04Xh",$stn{$pi}{'ODAaid'}{$gtype})) if ($dbg);
    }

  }

  given ($stn{$pi}{'ODAaid'}{$gtype}) {

    # Traffic Message Channel
    when ([0xCD46, 0xCD47]) {
      $stn{$pi}{'hasTMC'} = TRUE;
      &updateStatusRow;
      say sprintf("  ══╡ TMC sysmsg %04x",$_[2]);
    }

    # RT+
    when (0x4BD7) {
      $stn{$pi}{'hasRTplus'} = TRUE;
      $stn{$pi}{'rtp_which'} = ($_[2] >> 13) & _1BIT;
      $stn{$pi}{'CB'}        = ($_[2] >> 12) & _1BIT;
      $stn{$pi}{'SCB'}       = ($_[2] >> 8)  & _4BIT;
      $stn{$pi}{'templnum'}  =  $_[2]        & _8BIT;
      &updateStatusRow;
      say "  RT+ applies to ".($stn{$pi}{'rtp_which'} ? "enhanced RadioText" : "RadioText")        if ($dbg);
      say "  ".($stn{$pi}{'CB'} ? "Using template $stn{$pi}{'templnum'}" : "No template in use")   if ($dbg);
      say sprintf("  Server Control Bits: %Xh", $stn{$pi}{'SCB'})              if (!$stn{$pi}{'CB'} && $dbg);
    }

    # eRT
    when (0x6552) {
      $stn{$pi}{'haseRT'}     = TRUE;
      $stn{$pi}{'eRTbuf'}     = " " x 64 if (not exists $stn{$pi}{'eRTbuf'});
      $stn{$pi}{'ert_isutf8'} =  $_[2]       & _1BIT;
      $stn{$pi}{'ert_txtdir'} = ($_[2] >> 1) & _1BIT;
      $stn{$pi}{'ert_chrtbl'} = ($_[2] >> 2) & _4BIT;
      &updateStatusRow;
    }
    
    # Unimplemented ODA
    default {
      say "  ODAmsg: ". sprintf("%04x",$_[2])             if ($dbg);
      say "          Unimplemented Open Data Application" if ($dbg);
    }
  }
}

# 4A: Clock-time and date
    
sub Group4A {

  return if (@_ < 3);
  
  my $lto;
  my $mjd = (($_[1] & _2BIT) << 15) | (($_[2] >> 1) & _15BIT);

  if (@_ == 4) {
    # Local time offset
    $lto =  (($_[3] >> 0) & _5BIT) / 2;
    $lto = ((($_[3] >> 5) & _1BIT) ? -$lto : $lto);
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
      
    my $hr = ( ((($_[2] >> 0) & _1BIT) << 4) | (($_[3] >> 12) & _4BIT) + $lto) % 24;
    my $mn = ($_[3] >> 6) & _6BIT;

    say "  CT:     ". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13 && $hr > 0 && $hr < 24 && $mn > 0 && $mn < 60) ?
          sprintf("%04d-%02d-%02dT%02d:%02d%+03d:%02d", $yr, $mo, $dy, $hr, $mn, $lto, $ltom) :
          "Invalid datetime data") if ($dbg);
  } else {
    say "  CT:     ". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13) ?
          sprintf("%04d-%02d-%02d", $yr, $mo, $dy) :
          "Invalid datetime data") if ($dbg);
  }

}

# 5A: Transparent data channels or ODA

sub Group5A {

  return if (@_ < 4);

  my $addr = $_[1] & _5BIT;
  say "  TDChan: $addr" if ($dbg);
  say sprintf("  TDS:    %02x %02x %02x %02x",
    ($_[2]>>8) & _8BIT, ($_[2]>>0) & _8BIT, ($_[3]>>8) & _8BIT, ($_[3]>>0) & _8BIT) if ($dbg);
}

# 5B: Transparent data channels or ODA

sub Group5B {

  return if (@_ < 4);

  my $addr = $_[1] & _5BIT;
  say "  TDChan: $addr" if ($dbg);
  say sprintf("  TDS:    %02x %02x", ($_[3]>>8) & _8BIT, ($_[3]>>0) & _8BIT) if ($dbg);
}


# 6A: In-House Applications or ODA
    
sub Group6A {

  return if (@_ < 4);
  say "  IH:     ". sprintf("%02x %04x %04x", ($_[1] >> 0) & _5BIT, $_[2], $_[3]) if ($dbg);

}

# 6B: In-House Applications or ODA
    
sub Group6B {

  return if (@_ < 4);
  say "  IH:     ". sprintf("%02x %04x", ($_[1] >> 0) & _5BIT, $_[3]) if ($dbg);

}

# 7A: Radio Paging or ODA

sub Group7A {
  
  return if (@_ < 3);
  say sprintf("  ══╡ Pager 7A: %02x %04x %04x",$_[1] & _5BIT, $_[2], $_[3]);
}

# 9A: Emergency warning systems or ODA

sub Group9A {

  return if (@_ < 4);
  say "  EWS:    ". sprintf("%02x %04x %04x", ($_[1] >> 0) & _5BIT, $_[2], $_[3]) if ($dbg);

}

# 10A: Program Type Name (PTYN)

sub Group10A {

  if ((($_[1] >> 4) & _1BIT) != ($stn{$pi}{'PTYNAB'} // -1)) {
    say "         (A/B flag change, text reset)" if ($dbg);
    $stn{$pi}{'PTYN'} = " " x 8;
  }

  $stn{$pi}{'PTYNAB'} = ($_[1] >> 4) & _1BIT;

  if (@_ >= 3) {
    my @chr = ();
    $chr[0]  = ($_[2] >> 8) & _8BIT;
    $chr[1]  = ($_[2] >> 0) & _8BIT;

    if (@_ == 4) {
      $chr[2] = ($_[3] >> 8) & _8BIT;
      $chr[3] = ($_[3] >> 0) & _8BIT;
    }

    my $segaddr = ($_[1] >> 0) & _1BIT;

    substr($stn{$pi}{'PTYN'}, $segaddr*4 + $_, 1) = $charbasic[$chr[$_]] for (0..$#chr);
        
    say "  PTYN:   ", substr($stn{$pi}{'PTYN'},0,$segaddr*4).REVERSE.substr($stn{$pi}{'PTYN'},$segaddr*4,scalar(@chr)).
                RESET.substr($stn{$pi}{'PTYN'},$segaddr*4+scalar(@chr)) if ($dbg);
  }
}

# 13A: Enhanced Radio Paging or ODA

sub Group13A {
 
  return if (@_ < 4);
  say sprintf("  ══╡ Pager 13A: %02x %04x %04x",$_[1] & _5BIT, $_[2], $_[3]);

}

# 14A: Enhanced Other Networks (EON) information

sub Group14A {

  return if (@_ < 4);

  $stn{$pi}{'hasEON'}    = TRUE;
  my $eon_pi             =  $_[3];
  $stn{$eon_pi}{'TP'}    = ($_[1] >> 4) & _1BIT;
  my $eon_variant        = ($_[1] >> 0) & _4BIT;
  &updateStatusRow;
  say "  Other Network" if ($dbg);
  say "    PI:     ".sprintf("%04X",$eon_pi).((exists($stn{$eon_pi}{'chname'})) && " $stn{$eon_pi}{'chname'})") if ($dbg);
  say "    TP:     $TPtext[$stn{$eon_pi}{'TP'}]" if ($dbg);

  given ($eon_variant) {

    when ([0..3]) {
      print "  " if ($dbg);
      $stn{$eon_pi}{'PSbuf'} = " " x 8 unless (exists($stn{$eon_pi}{'PSbuf'}));
      &set_ps_khars($eon_pi, $eon_variant*2, ($_[2] >> 8) & _8BIT, ($_[2] >> 0) & _8BIT);
    }

    when (4) {
      say "    AF:     ".&parseAF(TRUE, ($_[2] >> 8) & _8BIT);
      say "    AF:     ".&parseAF(TRUE, ($_[2] >> 0) & _8BIT);
    }

    when ([5..8]) {
      say "    AF:     Tuned frequency ".&parseAF(TRUE, ($_[2] >> 8) & _8BIT)." maps to ".
                                         &parseAF(TRUE, ($_[2] >> 0) & _8BIT) if ($dbg);
    }

    when (9) {
      say "    AF:     Tuned frequency ".&parseAF(TRUE, ($_[2] >> 8) & _8BIT)." maps to ".
                                         &parseAF(FALSE,($_[2] >> 0) & _8BIT) if ($dbg);
    }

    when (12) {
      $stn{$eon_pi}{'LA'}  = ($_[2] >> 15) & _1BIT;
      $stn{$eon_pi}{'EG'}  = ($_[2] >> 14) & _1BIT;
      $stn{$eon_pi}{'ILS'} = ($_[2] >> 13) & _1BIT;
      $stn{$eon_pi}{'LSN'} = ($_[2] >> 1)  & _12BIT;
      if ($dbg && $stn{$eon_pi}{'LA'})  { say "    Link: Program is linked to linkage set ".sprintf("%03X",$stn{$eon_pi}{'LSN'}); }
      if ($dbg && $stn{$eon_pi}{'EG'})  { say "    Link: Program is member of an extended generic set"; }
      if ($dbg && $stn{$eon_pi}{'ILS'}) { say "    Link: Program is linked internationally"; }
      # TODO: Country codes, pg. 51
    }

    when (13) {
      $stn{$eon_pi}{'PTY'} = ($_[2] >> 11) & _5BIT;
      $stn{$eon_pi}{'TA'}  = ($_[2] >> 0)  & _1BIT;
      say "    PTY:    $stn{$eon_pi}{'PTY'} ".
        (exists $stn{$eon_pi}{'ECC'} && ($countryISO[$stn{$pi}{'ECC'}][$stn{$eon_pi}{'CC'}] // "") =~ /us|ca|mx/ ? 
          $ptynamesUS[$stn{$eon_pi}{'PTY'}] :
          $ptynames[$stn{$eon_pi}{'PTY'}]) if ($dbg);
      say "    TA:     $TAtext[$stn{$eon_pi}{'TP'}][$stn{$eon_pi}{'TA'}]"    if ($dbg);
    }

    when (14) {
      say "    PIN:    ". &parsepin($_[2]) if ($dbg);
    }

    when (15) {
      say "    Broadcaster data: ".sprintf("%04x", $_[2]) if ($dbg);
    }

    default {
      say "    EON variant $eon_variant is unallocated" if ($dbg);
    }

  }
}

# 14B: Enhanced Other Networks (EON) information

sub Group14B {

  return if (@_ < 4);

  my $eon_pi          =  $_[3];
  $stn{$eon_pi}{'TP'} = ($_[1] >> 4) & _1BIT;
  $stn{$eon_pi}{'TA'} = ($_[1] >> 3) & _1BIT;
  say "  Other Network"                                               if ($dbg);
  say "    PI:     ".sprintf("%04X",$eon_pi).((exists($stn{$eon_pi}{'chname'})) ? " ".$stn{$eon_pi}{'chname'} : "") if ($dbg);
  say "    TP:     $TPtext[$stn{$eon_pi}{'TP'}]"                      if ($dbg);
  say "    TA:     $TAtext[$stn{$eon_pi}{'TP'}][$stn{$eon_pi}{'TA'}]" if ($dbg);

}

# 15B: Fast basic tuning and switching information

sub Group15B {
  
  # DI
  my $DI_adr = 3 - ($_[1]   & _2BIT);
  my $DI     = ($_[1] >> 2) & _1BIT;
  &parseDI($DI_adr, $DI);

  # TA, M/S
  $stn{$pi}{'TA'} = ($_[1] >> 4) & _1BIT;
  $stn{$pi}{'MS'} = ($_[1] >> 3) & _1BIT;
  say "  TA:     $TAtext[$stn{$pi}{'TP'}][$stn{$pi}{'TA'}]" if ($dbg);
  say "  M/S:    ".qw( Speech Music )[$stn{$pi}{'MS'}]    if ($dbg);

  $stn{$pi}{'hasMS'} = TRUE;
  &updateStatusRow;

}

# Any group used for Open Data

sub ODAGroup {

  my ($gtype, @data) = @_;
  
  return if (@data < 4);

  if (exists $stn{$pi}{'ODAaid'}{$gtype}) {
    given ($stn{$pi}{'ODAaid'}{$gtype}) {

      when ([0xCD46, 0xCD47]) { say sprintf("  ══╡ TMC msg %02x %04x %04x",
                                $data[1] & _5BIT, $data[2], $data[3]); }

      when (0x4BD7)           { &parse_RTp(@data); }

      when (0x6552)           { &parse_eRT(@data); }

      default                 { say sprintf("          Unimplemented ODA %04x: %02x %04x %04x",
                                    $stn{$pi}{'ODAaid'}{$gtype}, $data[1] & _5BIT, $data[2], $data[3])
                                    if ($dbg); }

    }
  } else {
    say "          Will need group 3A first to identify ODA" if ($dbg);
  }
}

sub screenReset {

  $stn{$pi}{'RTbuf'}       = (" " x 64) if (!exists  $stn{$pi}{'RTbuf'});
  $stn{$pi}{'RTmarkup'}[0] = (" " x 32) if (!defined $stn{$pi}{'RTmarkup'}[0]);
  $stn{$pi}{'RTmarkup'}[1] = (" " x 32) if (!defined $stn{$pi}{'RTmarkup'}[1]);
  $stn{$pi}{'hasRT'}       = FALSE      if (!exists  $stn{$pi}{'hasRT'});
  $stn{$pi}{'hasMS'}       = FALSE      if (!exists  $stn{$pi}{'hasMS'});
  $stn{$pi}{'TP'}          = FALSE      if (!exists  $stn{$pi}{'TP'});
  $stn{$pi}{'TA'}          = FALSE      if (!exists  $stn{$pi}{'TA'});

  $PSlabel   ->set_markup("<span font='Mono Bold 25px'>        </span>");
  $PIlabel   ->set_markup("<span font='mono 12px'>".(defined($pi) ? sprintf("%04X",$pi) : "    ")."</span>");
  $ECClabel  ->set_markup("<span font='Mono 11px' foreground='$themef{dim}'>  </span>");
  $PTYlabel  ->set_markup("<span font='Mono 12px'>".(" " x 16)."</span>");
  $RTlabel[0]->set_markup("<span font='Mono 15px' foreground='$themef{fg}'>".$stn{$pi}{'RTmarkup'}[0]."</span>");
  $RTlabel[1]->set_markup("<span font='Mono 15px' foreground='$themef{fg}'>".$stn{$pi}{'RTmarkup'}[1]."</span>");
  $Freqlabel ->set_markup("<span font='mono 12px' foreground='$themef{fg}'>".($stn{$pi}{'freq'} // "    ")."</span>");
  &updateStatusRow;

}

sub on_quit_button_clicked {
  system("killall -9 downmix gs");
  Gtk2->main_quit;
}

# Change characters in RadioText

sub set_rt_khars {
  (my $lok, my @a) = @_;

  $stn{$pi}{'hasRT'} = TRUE;
  &updateStatusRow;

  for $i (0..$#a) {
    given ($a[$i]) {
      when (0x0D) { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = "↵";                }
      when (0x0A) { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = "␊";                }
      default     { substr($stn{$pi}{'RTbuf'}, $lok+$i, 1) = $charbasic[$a[$i]]; }
    }
    $stn{$pi}{'RTrcvd'}[$lok+$i] = TRUE;
  }

  # Message will be displayed when fully received
  
  my $minRTlen = ($stn{$pi}{'RTbuf'} =~ /↵/ ? index($stn{$pi}{'RTbuf'},"↵") + 1 : $stn{$pi}{'presetminRTlen'} // 0);
  
  my $totrc = grep (defined $_, @{$stn{$pi}{'RTrcvd'}}[0..$minRTlen]) if ($minRTlen > 0);

  if ($minRTlen == 0 || $totrc >= $minRTlen) {
    for (0..1) {
      ($stn{$pi}{'RTmarkup'}[$_] =  substr($stn{$pi}{'RTbuf'},$_*32,32)) =~ s/&/&amp;/g;
      $stn{$pi}{'RTmarkup'}[$_]  =~ s/</&lt;/g;
      $stn{$pi}{'RTmarkup'}[$_]  =~ s/↵/ /g;
      $RTlabel[$_]->set_markup("<span font='Mono 15px' foreground='$themef{fg}'>$stn{$pi}{'RTmarkup'}[$_]</span>");
    }
  }

  say "  RT:     ". substr($stn{$pi}{'RTbuf'},0,$lok).REVERSE.substr($stn{$pi}{'RTbuf'},$lok,scalar(@a)).RESET.
                    substr($stn{$pi}{'RTbuf'},$lok+scalar(@a)) if ($dbg);

  say "          ". join("", (map ((defined) ? "^" : " ", @{$stn{$pi}{'RTrcvd'}}[0..63]))) if ($dbg);
}

# Enhanced RadioText

sub parse_eRT {
  my $addr = $_[1] & _5BIT;

  if ($stn{$pi}{'ert_chrtbl'} == 0x00 &&
     !$stn{$pi}{'ert_isutf8'}         &&
      $stn{$pi}{'ert_txtdir'} == 0) {
    
    for (0..1) {
      substr($stn{$pi}{'eRTbuf'}, 2*$addr+$_, 1) = decode("UCS-2LE", chr( ($_[2+$_]>>8) & _8BIT).chr($_[2+$_] & _8BIT));
      $stn{$pi}{'eRTrcvd'}[2*$addr+$_]           = TRUE;
    }
  
    say "  eRT:    ". substr($stn{$pi}{'eRTbuf'},0,2*$addr).REVERSE.substr($stn{$pi}{'eRTbuf'},2*$addr,2).RESET.
                      substr($stn{$pi}{'eRTbuf'},2*$addr+2) if ($dbg);
  
    say "          ". join("", (map ((defined) ? "^" : " ", @{$stn{$pi}{'eRTrcvd'}}[0..63]))) if ($dbg);

  }
}

# Change characters in the Program Service name

sub set_ps_khars {
  my $pspi = $_[0];
  my $lok  = $_[1];
  my @khar = ($_[2], $_[3]);
  my $markup;
    
  $stn{$pspi}{'PSbuf'} = " " x 8 if (not exists $stn{$pspi}{'PSbuf'});

  substr($stn{$pspi}{'PSbuf'}, $lok, 2) = $charbasic[$khar[0]] . $charbasic[$khar[1]];

  # Display PS name when received without gaps

  $stn{$pspi}{'prevPSlok'}      = 0  if (not exists $stn{$pspi}{'prevPSlok'});
  $stn{$pspi}{'PSrcvd'}         = () if ($lok != $stn{$pspi}{'prevPSlok'} + 2 || $lok == $stn{$pspi}{'prevPSlok'});
  $stn{$pspi}{'PSrcvd'}[$lok/2] = TRUE;
  $stn{$pspi}{'prevPSlok'}      = $lok;
  my $totrc                     = grep (defined, @{$stn{$pspi}{'PSrcvd'}}[0..3]);

  if ($totrc == 4) {
    ($markup = $stn{$pspi}{'PSbuf'}) =~ s/&/&amp;/g;
    $markup =~ s/</&lt;/g;
    $PSlabel->set_markup("<span font='Mono Bold 25px' foreground='$themef{fg}'>$markup</span>") if ($pspi == $pi);
  }

  say "  PS:     ". substr($stn{$pspi}{'PSbuf'},0,$lok).REVERSE.substr($stn{$pspi}{'PSbuf'},$lok,2).RESET.
                    substr($stn{$pspi}{'PSbuf'},$lok+2) if ($dbg);

}

# RadioText+

sub parse_RTp {

  my @ctype;
  my @start;
  my @len;

  # P.5.2
  my $itog  = ($_[1] >> 4) & _1BIT;
  my $irun  = ($_[1] >> 3) & _1BIT;
  $ctype[0] = (($_[1] & _3BIT) << 3) + (($_[2] >> 13) & _3BIT);
  $ctype[1] = (($_[2] & _1BIT) << 5) + (($_[3] >> 11) & _5BIT);
  $start[0] = ($_[2] >> 7) & _6BIT;
  $start[1] = ($_[3] >> 5) & _6BIT;
  $len[0]   = ($_[2] >> 1) & _6BIT;
  $len[1]   =  $_[3] & _5BIT;

  say "  RadioText+: " if ($dbg);

  if ($irun) {
    say "    Item running" if ($dbg);
    if ($stn{$pi}{'rtp_which'} == 0) {
      for $tag (0..1) {
        my $totrc = grep (defined $_, @{$stn{$pi}{'RTrcvd'}}[$start[$tag]..($start[$tag]+$len[$tag]-1)]);
        if ($totrc == $len[$tag]) {
          say "    Tag $rtpclass[$ctype[$tag]]: ".substr($stn{$pi}{'RTbuf'}, $start[$tag], $len[$tag]) if ($dbg);
        }
      }
    } else {
      # (eRT)
    }
  } else {
    say "    No item running" if ($dbg);
  }

}

# Program Item Number

sub parsepin {
  my $d   = ($_[0] >> 11) & _5BIT;
  return ($d ? sprintf("Day %d at %02d:%02d",$d, ($_[0] >> 6)  & _5BIT, ($_[0] >> 0)  & _6BIT ) : "Not in use");
}

# Decoder Identification

sub parseDI {
  if ($dbg) {
    given ($_[0]) {
      when (0) { say "  DI:     ". qw( Mono Stereo )[$_[1]];           }
      when (1) { say "  DI:     Artificial head" if ($_[1]);           }
      when (2) { say "  DI:     Compressed"      if ($_[1]);           }
      when (3) { say "  DI:     ". qw( Static Dynamic)[$_[1]] ." PTY"; }
    }
  }
}

# Alternate Frequencies

sub parseAF {
  my $fm  = shift;
  my $num = shift;
  if ($fm) {
    given ($num) {
      when ([1..204])   { return sprintf("%0.1f MHz", (87.5 + $num/10) ); }
      when (205)        { return "(filler)"; }
      when (224)        { return "No AF exists"; }
      when ([225..249]) { return ($num == 225 ? "One AF follows" : ($num-224)." AFs follow"); }
      when (250)        { return "AM/LF freq follows"; }
      default           { return "(error: $num)"; }
    }
  } else {
    given ($num) {
      when ([1..15])    { return sprintf("%d kHz", 144 + $num * 9); }
      when ([16..135])  { return sprintf("%d kHz", 522 + ($num-15) * 9); }
      default           { return "N/A"; }
    }
  }
}

sub updateStatusRow {

  $AppLabel->set_markup("<span font='Sans Italic 9px'>".
    "<span foreground='".$themef{$stn{$pi}{'hasRT'}     ? "fg" : "dim"}."'>RadioText</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'hasRTplus'} ? "fg" : "dim"}."'>RT+</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'haseRT'}    ? "fg" : "dim"}."'>eRT</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'hasEON'}    ? "fg" : "dim"}."'>EON</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'hasTMC'}    ? "fg" : "dim"}."'>TMC</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'TP'}        ? "fg" : "dim"}."'>TP</span>  ".
    "<span foreground='".$themef{$stn{$pi}{'TA'}        ? "fg" : "dim"}."'>TA</span></span>");

    $Siglabel->set_markup("<span font='Mono 11px'><span foreground='$themef{fg}'>".("⟫" x ($qty // 0)) .
      "</span><span foreground='$themef{dim}'>".("⟫" x (5-($qty // 0)))."</span></span>");
    
    $RDSlabel->set_markup("<span font='Mono 7px' foreground='".$themef{$insync ? "fg" : "dim"}.
      "'>RDS</span>");
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

  # Theme
  open (INFILE, "themes/theme-".$theme) or die ($!);
  for (<INFILE>) {
    chomp();
    ($a,$b) = split(/=/,$_);
    $themef{$a} = $b;
  }
  close(INFILE);
  
  # Saved station settings
  open (INFILE, "stations") or die ($!);
  for (<INFILE>) {
    chomp;
    if (/^([\dA-F]{4}) \"(.{8})\" ([01]) ([01]) (\d+) +(.+)/i) {
      ($stn{hex($1)}{'presetPSbuf'}, $stn{hex($1)}{'denyRTAB'}, $stn{hex($1)}{'denyPS'}, $stn{hex($1)}{'presetminRTlen'}, $stn{hex($1)}{'chname'}) =
        ($2, $3, $4, $5, $6);
      die ("minRTlen must be <=64") if ($stn{hex($1)}{'presetminRTlen'} > 64);
    }
  }
  close(INFILE);

}

sub initgui {

  # Initialize GUI
  
  my $window = Gtk2::Window->new ("toplevel");
  $window-> set_title            ("redsea" );
  $window-> set_border_width     (10);
  $window-> signal_connect       (delete_event => sub {Gtk2->main_quit; FALSE});
  
  my $table = Gtk2::Table->new   (3, 4, FALSE);
  $window->add                   ($table);
  
  ## ODA app row
  $AppLabel= Gtk2::Label->new  ();
  $AppLabel->set_markup        ("<span font='Sans Italic 9px' foreground='$themef{dim}'>RadioText  RT+  eRT  EON  TMC  TP  TA</span>");
  $table   ->attach_defaults   ($AppLabel, 0,4,0,1);
  
  ## PS name
  $PSlabel = Gtk2::Label->new  ();
  $PSlabel ->set_markup        ("<span font='Mono Bold 25px'>        </span>");
  $PSlabel ->set_width_chars   (16);
  $table   ->attach_defaults   ($PSlabel, 0,1,1,2);
  
  ## Misc info table
  my $infotable = Gtk2::Table->new (5, 2, FALSE);
  $table        ->attach_defaults  ($infotable, 1,2,1,2);
  
  ## PI
  my $label = Gtk2::Label->new ();
  $label    ->set_markup       ("<span font='mono 7px' foreground='$themef{dim}'>PI</span>");
  $infotable->attach_defaults  ($label, 0,1,0,1);
  $PIlabel  = Gtk2::Label->new ();
  $PIlabel  ->set_markup       ("<span font='mono 12px'>    </span>");
  $PIlabel  ->set_alignment    (0, .5);
  $PIlabel  ->set_width_chars  (4);
  $infotable->attach_defaults  ($PIlabel, 1,2,0,1);
  
  ## MHz
  $label    = Gtk2::Label->new ();
  $label    ->set_markup       ("<span font='mono 7px' foreground='$themef{dim}'>MHz</span>");
  $infotable->attach_defaults  ($label, 0,1,1,2);
  $Freqlabel= Gtk2::Label->new ();
  $Freqlabel->set_markup       ("<span font='mono 12px'>    </span>");
  $Freqlabel->set_alignment    (0, .5);
  $Freqlabel->set_width_chars  (5);
  $infotable->attach_defaults  ($Freqlabel, 1,2,1,2);
  
  ## Signal quality meter
  $RDSlabel = Gtk2::Label->new ();
  $RDSlabel ->set_markup       ("<span font='mono 7px' foreground='$themef{dim}'>RDS</span>");
  $infotable->attach_defaults  ($RDSlabel, 2,3,0,1);
  $Siglabel = Gtk2::Label->new ();
  $Siglabel ->set_markup       ("<span font='Mono 11px' foreground='$themef{dim}'>     </span>");
  $Siglabel ->set_alignment    (0, .5);
  $infotable->attach_defaults  ($Siglabel, 3,4,0,1);
  
  ## ECC
  $label    = Gtk2::Label->new ();
  $label    ->set_markup       ("<span font='mono 7px' foreground='$themef{dim}'>ECC</span>");
  $infotable->attach_defaults  ($label, 4,5,0,1);
  $ECClabel = Gtk2::Label->new ();
  $ECClabel ->set_markup       ("<span font='Mono 11px' foreground='$themef{dim}'>  </span>");
  $ECClabel ->set_alignment    (0, .5);
  $infotable->attach_defaults  ($ECClabel, 5,6,0,1);
  
  ## PTY
  $label    = Gtk2::Label->new ();
  $label    ->set_markup       ("<span font='mono 7px' foreground='$themef{dim}'>PTY</span>");
  $infotable->attach_defaults  ($label, 2,3,1,2);
  $PTYlabel = Gtk2::Label->new ();
  $PTYlabel ->set_markup       ("<span font='Mono 12px'>".(" " x 16)."</span>");
  $PTYlabel ->set_width_chars  (16);
  $PTYlabel ->set_alignment    (0, .5);
  $infotable->attach_defaults  ($PTYlabel, 3,6,1,2);
  
  ## RadioText
  for (0..1) {
    $RTlabel[$_] = Gtk2::Label->new ();
    $RTlabel[$_] ->set_markup       ("<span font='Mono 15px'>".(" " x 32)."</span>");
    $RTlabel[$_] ->set_alignment    (.5, .5);
    $RTlabel[$_] ->set_width_chars  (32);
    $table       ->attach_defaults  ($RTlabel[$_], 0,3,2+$_,3+$_);
  }
  
  my $back_pixbuf    =  Gtk2::Gdk::Pixbuf->new_from_file("themes/".$themef{bgpic});
  my ($pixmap,$mask) = $back_pixbuf->render_pixmap_and_mask(255);
  my $style = $window->get_style();
  $style    = $style->copy      ();
  $style    ->bg_pixmap         ("normal",$pixmap);
  $window   ->set_style         ($style);
  
  #$window->set_resizable   (FALSE);
  $window->set_default_size (324, 118);
  $window->resize           (324, 118);
  
  $window->show_all();
}
