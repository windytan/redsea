#!/usr/bin/perl

#
# redsea - RDS decoder
# Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#

use 5.012;
use strict;
use warnings;
use utf8;
no if $] >= 5.017011, warnings => 'experimental::smartmatch';

use IPC::Cmd     qw/can_run/;
use Encode       qw/decode/;
use POSIX        qw/strftime/;
use Getopt::Std;

binmode(STDOUT, ':encoding(UTF-8)');

$| ++;

# DSP frequencies
use constant FS => 250_000;

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

my @ofs_letters = qw( A B C C' D );

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
use constant     RED => "\x1B[31m";
use constant   GREEN => "\x1B[32m";
use constant    GRAY => "\x1B[30;1m";

my @group_buffer;
my @group_data;
my @has_block;
my @block_has_errors;
my %options;
my %station;
my $source_file;

my @countryISO, my @group_names, my @ptynamesUS, my @ptynames;
my @TA_descr, my @TP_descr, my @langname, my @oda_app, my @char_table;
my @rtpclass, my @error_lookup;
my $block_counter;
my $newpi, my $ednewpi;

my $pi = 0;
my $is_in_sync = FALSE;
my $verbosity = 0;
my $expected_offset;
my $rtl_pid;

my $debug = FALSE;

my $is_scanning = FALSE;
my $scan_seconds = 5;

my $is_interactive = (-t STDOUT ? TRUE : FALSE);

my $bitpipe;
my $linebuf;
my $fmfreq;


init_data();
get_options();

if ($is_scanning) {
  my $f;
  for ($f = 87.9; $f < 106.5; $f += .1) {
    printf("%.1f: ", $f);
    open_radio($f * 1e6);
    get_groups();
    printf("%04x\n",$pi);
  }
} else {
  open_radio($fmfreq);
  get_groups();
}

sub dbg {
  if ($debug) {
    say $_[0];
  }
}

sub print_usage {
  my $usage = <<"END_USAGE";
Usage: $0 [-hlst] [-p <error>] FREQ

    -h          display this help and exit
    -l          print groups in long format
    -s          print groups in short format (default)
    -t          print an ISO timestamp before each group
    -p <error>  parts-per-million error, passed to rtl_fm (optional;
                allows for faster PLL lock if set correctly)
    FREQ        station frequency in Hz, can be SI suffixed
                (94.0M)

END_USAGE

  print $usage;
}

sub get_options {

  getopts('hlstp:df:', \%options);


  if (exists $options{l}) {
    $verbosity = 1;
  }
  if (exists $options{d}) {
    $debug = TRUE;
  }
  if (exists $options{f}) {
    $source_file = $options{f};
  }

  $fmfreq = $ARGV[0] // q{};

  if (exists $options{h} || (not defined $source_file and
      $fmfreq !~ /^[\d\.]+[kMG]?$/i)) {
    print_usage();
    exit();
  }


  if (not defined $source_file) {
    if ($fmfreq =~ /^([\d\.]+)([kMG])$/i) {
      my %si = ( 'k' => 1e3, 'K' => 1e3, 'm' => 1e6,
                 'M' => 1e6, 'g' => 1e9, 'G' => 1e9 );
      $fmfreq = $1 * $si{$2};
    }

    # sensible guess
    if ($fmfreq < 200e3) {
      say 'Note: assuming '.sprintf('%.2f', $fmfreq).' MHz';
      $fmfreq *= 1e6;
    }
  }

}

sub open_radio {
  my $freq = shift;

  if (defined $source_file) {
    $rtl_pid = open $bitpipe, '-|', 'sox '.$source_file.' -r 250000 -c 1 '.
               '-t .s16 - | ./rtl_redsea';

  } else {

    my $ppm  =
      (exists $options{p} ? sprintf(' -p %.0f ', $options{p}) : q{});

    my $rtl_redsea_exe;
    my $rtl_fm_exe;

    if      (can_run './rtl_redsea') {
      $rtl_redsea_exe = './rtl_redsea';
    } elsif (can_run 'rtl_redsea.exe') {
      $rtl_redsea_exe = 'rtl_redsea.exe';
    } else {
      print "error: looks like rtl_redsea isn't compiled. To fix that, ".
            "please run:\n\n".
            "gcc -std=gnu99 -o rtl_redsea rtl_redsea.c -lm\n";
      exit(1);
    }

    if      (can_run('rtl_fm')) {
      $rtl_fm_exe = 'rtl_fm';
    } elsif (can_run('rtl_fm.exe')) {
      $rtl_fm_exe = 'rtl_fm.exe';
    } else {
      print "error: looks like rtl_fm is not installed!\n";
      exit(1);
    }

    $rtl_pid
      = open $bitpipe, '-|', sprintf($rtl_fm_exe.' -f %.1f -M fm -l 0 '.
                       '-A std '.$ppm.' -F 9 -s %.1f | '.$rtl_redsea_exe,
                       $freq, FS) or die($!);
  }
}

# Next bit from radio
sub get_bit {
  read $bitpipe, my $bit, 1 or die 'End of stream';
  return $bit;
}

# Calculate the syndrome of a 26-bit vector
sub syndrome {
  my $vector = shift;

  my ($l, $bit);
  my $synd_reg = 0x000;

  for my $k (reverse(0..25)) {
    $bit       = ($vector  & (1 << $k));
    $l         = ($synd_reg & 0x200);      # Store lefmost bit of register
    $synd_reg  = ($synd_reg << 1) & 0x3FF; # Rotate register
    $synd_reg ^= ($bit ? 0x31B : 0x00);    # Premult. input by x^325 mod g(x)
    $synd_reg ^= ($l   ? 0x1B9 : 0x00);    # Division mod 2 by g(x)
  }

  return $synd_reg;
}


# When a block has uncorrectable errors, dump the group received so far
sub blockerror {
  dbg(GRAY."offset $ofs_letters[$expected_offset] not received".RESET);
  my $data_length = 0;

  if ($has_block[A]) {
    $data_length = 1;

    if ($has_block[B]) {
      $data_length = 2;

      if ($has_block[C] || $has_block[Ci]) {
        $data_length = 3;
      }
    }
    my @new_group;
    @new_group = @group_data[0..$data_length-1];
    push (@group_buffer, \@new_group);
  } elsif ($has_block[Ci]) {
    my @new_group;
    @new_group = $group_data[2];
    push (@group_buffer, \@new_group);
  }

  $block_has_errors[$block_counter % 50] = TRUE;

  my $erroneous_blocks = 0;
  for (@block_has_errors) {
    $erroneous_blocks += ($_ // 0);
  }

  # Sync is lost when >45 out of last 50 blocks are erroneous (C.1.2)
  if ($is_in_sync && $erroneous_blocks > 45) {
    $is_in_sync       = FALSE;
    @block_has_errors = ();
    $pi               = 0;
    dbg (RED."Sync lost (45 errors out of 50)".RESET);
  }

  @has_block = ();
}

sub get_groups {

  my $block = my $wideblock = my $bitcount = my $prevbitcount = 0;
  my ($dist, $message);
  my $pi = my $i = 0;
  my $j = my $data_length = my $buf = my $prevsync = 0;
  my $left_to_read = 26;
  my @has_sync_for;

  my @offset_word = (0x0FC, 0x198, 0x168, 0x350, 0x1B4);
  my @ofs2block   = (0, 1, 2, 2, 3);
  my $synd_reg;

  if (not defined $source_file) {
    print STDERR 'Waiting for sync at '.sprintf('%.2f', $fmfreq / 1e6)." MHz\n";
  }

  my $start_time = time();

  while (TRUE) {

    if ($is_scanning) {
      my $elapsed_time = time() - $start_time;
      if ($elapsed_time >= $scan_seconds) {
        close $bitpipe;
        sleep 1;
        kill $rtl_pid;
        sleep 1;
        last;
      }
    }

    # Compensate for clock slip corrections
    $bitcount += 26-$left_to_read;

    # Read from radio
    for ($i=0; $i < ($is_in_sync ? $left_to_read : 1); $i++, $bitcount++) {
      $wideblock = ($wideblock << 1) + get_bit();
    }

    $left_to_read = 26;
    $wideblock &= _28BIT;

    $block = ($wideblock >> 1) & _26BIT;

    # Find the offsets for which the syndrome is zero
    for (A .. D) {
      $has_sync_for[$_] = (syndrome($block ^ $offset_word[$_]) == 0);
    }

    # Acquire sync

    if (!$is_in_sync) {

      if ($has_sync_for[A] | $has_sync_for[B] | $has_sync_for[C] |
          $has_sync_for[Ci] | $has_sync_for[D]) {

        BLOCKS:
        for my $bnum (A .. D) {
          if ($has_sync_for[$bnum]) {
            $dist = $bitcount - $prevbitcount;

            if ($dist % 26 == 0 && $dist <= 156 &&
               ($ofs2block[$prevsync] + $dist/26) % 4 == $ofs2block[$bnum]) {
              $is_in_sync      = TRUE;
              $expected_offset = $bnum;
              dbg (GRAY."sync acquired: correct offset ".$ofs_letters[$bnum].
                   " repeated at interval ".$dist.RESET);
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

    if ($is_in_sync) {

      $block_counter ++;

      $message = $block >> 10;

      # If expecting C but we only got a Ci sync pulse, we have a Ci block
      if ($expected_offset == C && !$has_sync_for[C] &&
          $has_sync_for[Ci]) {
        $expected_offset = Ci;
      }

      # If this block offset won't give a sync pulse
      if (not $has_sync_for[$expected_offset]) {

        # If it's a correct PI, the error was probably in the check bits and
        # hence is ignored
        if      ($expected_offset == A && $message == $pi && $pi != 0) {
          $has_sync_for[A]  = TRUE;
          dbg(GREEN."ignoring error in check bits".RESET);
        } elsif ($expected_offset == C && $message == $pi && $pi != 0) {
          $has_sync_for[Ci] = TRUE;
          dbg(GREEN."ignoring error in check bits".RESET);
        }

        # Detect & correct clock slips (C.1.2)

        elsif   ($expected_offset == A && $pi != 0 &&
                (($wideblock >> 12) & _16BIT ) == $pi) {
          $message           = $pi;
          $wideblock       >>= 1;
          $has_sync_for[A] = TRUE;
          dbg(GREEN."clock slip corrected".RESET);
        } elsif ($expected_offset == A && $pi != 0 &&
                (($wideblock >> 10) & _16BIT ) == $pi) {
          $message           = $pi;
          $wideblock         = ($wideblock << 1) + get_bit();
          $has_sync_for[A] = TRUE;
          $left_to_read      = 25;
          dbg(GREEN."clock slip corrected".RESET);
        } else {

          # Detect & correct burst errors (B.2.2)

          $synd_reg = syndrome($block ^ $offset_word[$expected_offset]);

          if ($pi != 0 && $expected_offset == 0) {
            dbg(GRAY.sprintf("%03x expecting PI=%04x(%016b), ".
                "got %04x(%016b), xor=%016b", $synd_reg,
                $pi,$pi,($block>>10),($block>>10), $pi ^ ($block>>10)).RESET);
          }

          if (defined $error_lookup[$synd_reg]) {
            $message = ($block >> 10) ^ $error_lookup[$synd_reg];
            $has_sync_for[$expected_offset] = TRUE;

            dbg(GREEN.sprintf("%03x error-corrected ".
               $ofs_letters[$expected_offset]." using ".
              "vector %016b", $synd_reg, $error_lookup[$synd_reg]).RESET);
          } else {
            dbg(RED.sprintf("%03x uncorrectable",$synd_reg).RESET);
          }
        }

        # If still no sync pulse
        if (not $has_sync_for[$expected_offset]) {
          blockerror ();
        }
      }

      # Error-free block received
      if ($has_sync_for[$expected_offset]) {

        dbg (GRAY."offset $ofs_letters[$expected_offset], correct message ".
          sprintf("%04x", $message).RESET);

        $group_data[$ofs2block[$expected_offset]] = $message;
        $block_has_errors[$block_counter % 50]    = FALSE;
        $has_block[$expected_offset]              = TRUE;

        if ($expected_offset == A) {
          $pi = $message;
        }

        # A complete group is received
        if ($has_block[A] && $has_block[B] &&
           ($has_block[C] || $has_block[Ci]) && $has_block[D]) {
          decode_group(@group_data);
        }
      }

      # The block offset we're expecting next
      $expected_offset = ($expected_offset == C ? D :
        ($expected_offset + 1) % 5);

      if ($expected_offset == A) {
        @has_block = ();
      }
    }
  }
  return FALSE;
}


sub decode_group {

  my @blocks = @_;

  return if ($blocks[0] == 0x0000);
  my ($group_type, $full_group_type);

  $ednewpi = ($newpi // 0);
  $newpi   = $blocks[0];

  if (exists $options{t}) {
    my $timestamp = strftime('%Y-%m-%dT%H:%M:%S%z ', localtime);
    utter ($timestamp, $timestamp);
  }

  if (@blocks >= 2) {
    $group_type       = extract_bits($blocks[1], 11, 5);
    $full_group_type  = extract_bits($blocks[1], 12, 4).
                       (extract_bits($blocks[1], 11, 1) ? 'B' : 'A' );
  } else {
    utter ('(PI only)', q{});
  }

  utter (('  PI:     '.sprintf('%04X',$newpi).
    ((exists($station{$newpi}{chname})) ?
      q{ }.$station{$newpi}{chname} : q{}),
      sprintf('%04X',$newpi)));

  # PI is repeated -> confirmed
  if ($newpi == $ednewpi) {

    # PI has changed from last confirmed
    if ($newpi != ($pi // 0)) {
      $pi = $newpi;
      screenReset();
    }

  } elsif ($newpi != ($pi // 0)) {
    utter ('          (repeat will confirm PI change)',"?\n");
    return;
  }

  # Nothing more to be done for PI only
  if (@blocks == 1) {
    utter ("\n","\n");
    return;
  }

  utter (
   (@blocks == 4 ? "Group $full_group_type: $group_names[$group_type]" :
                   "(partial group $full_group_type, ".scalar(@blocks).
                   ' blocks)'),
   (@blocks == 4 ? sprintf(' %3s', $full_group_type) :
                   sprintf(' (%3s)', $full_group_type)));

  # Traffic Program (TP)
  $station{$pi}{TP} = extract_bits($blocks[1], 10, 1);
  utter ('  TP:     '.$TP_descr[$station{$pi}{TP}],
         ' TP:'.$station{$pi}{TP});

  # Program Type (PTY)
  $station{$pi}{PTY} = extract_bits($blocks[1], 5, 5);

  if (exists $station{$pi}{ECC} &&
     ($countryISO[$station{$pi}{ECC}][$station{$pi}{CC}] // q{})
       =~ /us|ca|mx/) {
    utter ("  PTY:    ". sprintf("%02d",$station{$pi}{PTY}).
           q{ }.$ptynamesUS[$station{$pi}{PTY}],
           ' PTY:'.sprintf('%02d',$station{$pi}{PTY}));
  } else {
    utter ('  PTY:    '. sprintf('%02d',$station{$pi}{PTY}).
           q{ }.$ptynames[$station{$pi}{PTY}],
           ' PTY:'.sprintf('%02d',$station{$pi}{PTY}));
  }

  # Data specific to the group type

  given ($group_type) {
    when (0)  {
      Group0A (@blocks);
    }
    when (1)  {
      Group0B (@blocks);
    }
    when (2)  {
      Group1A (@blocks);
    }
    when (3)  {
      Group1B (@blocks);
    }
    when (4)  {
      Group2A (@blocks);
    }
    when (5)  {
      Group2B (@blocks);
    }
    when (6)  {
      exists ($station{$pi}{ODAaid}{6})  ?
        ODAGroup(6, @blocks)  : Group3A (@blocks);
      }
    when (8)  {
      Group4A (@_);
    }
    when (10) {
      exists ($station{$pi}{ODAaid}{10}) ?
        ODAGroup(10, @blocks) : Group5A (@blocks);
    }
    when (11) {
      exists ($station{$pi}{ODAaid}{11}) ?
        ODAGroup(11, @blocks) : Group5B (@blocks);
    }
    when (12) {
      exists ($station{$pi}{ODAaid}{12}) ?
        ODAGroup(12, @blocks) : Group6A (@blocks);
    }
    when (13) {
      exists ($station{$pi}{ODAaid}{13}) ?
        ODAGroup(13, @blocks) : Group6B (@blocks);
    }
    when (14) {
      exists ($station{$pi}{ODAaid}{14}) ?
        ODAGroup(14, @blocks) : Group7A (@blocks);
    }
    when (18) {
      exists ($station{$pi}{ODAaid}{18}) ?
        ODAGroup(18, @blocks) : Group9A (@blocks);
    }
    when (20) {
      Group10A(@blocks);
    }
    when (26) {
      exists ($station{$pi}{ODAaid}{26}) ?
        ODAGroup(26, @blocks) : Group13A(@blocks);
    }
    when (28) {
      Group14A(@blocks);
    }
    when (29) {
      Group14B(@blocks);
    }
    when (31) {
      Group15B(@blocks);
    }

    default   {
      ODAGroup($group_type, @blocks);
    }
  }

  utter("\n","\n");

}

# 0A: Basic tuning and switching information

sub Group0A {

  my @blocks = @_;

  # DI
  my $DI_adr = 3 - extract_bits($blocks[1], 0, 2);
  my $DI     = extract_bits($blocks[1], 2, 1);
  print_DI($DI_adr, $DI);

  # TA, M/S
  $station{$pi}{TA} = extract_bits($blocks[1], 4, 1);
  $station{$pi}{MS} = extract_bits($blocks[1], 3, 1);
  utter ('  TA:     '.
    $TA_descr[$station{$pi}{TP}][$station{$pi}{TA}],
    ' TA:'.$station{$pi}{TA});
  utter ('  M/S:    '.qw( Speech Music )[$station{$pi}{MS}],
    ' MS:'.qw(S M)[$station{$pi}{MS}]);

  $station{$pi}{hasMS} = TRUE;

  if (@blocks >= 3) {
    # AF
    my @af;
    for (0..1) {
      $af[$_] = parse_AF(TRUE, extract_bits($blocks[2], 8-$_*8, 8));
      utter ('  AF:     '.$af[$_],' AF:'.$af[$_]);
    }
    if ($af[0] =~ /follow/ && $af[1] =~ /Hz/) {
      ($station{$pi}{freq} = $af[1]) =~ s/ ?[kM]Hz//;
    }
  }

  if (@blocks == 4) {

    # Program Service Name (PS)

    if ($station{$pi}{denyPS}) {
      utter ("          (Ignoring changes to PS)"," denyPS");
    } else {
      set_PS_chars($pi, extract_bits($blocks[1], 0, 2) * 2,
        extract_bits($blocks[3], 8, 8), extract_bits($blocks[3], 0, 8));
      if ($station{$pi}{numPSrcvd} == 4) {
        utter (q{}, ' PS_OK');
      }
    }
  }
}

# 0B: Basic tuning and switching information

sub Group0B {

  my @blocks = @_;

  # Decoder Identification
  my $DI_adr = 3 - extract_bits($blocks[1], 0, 2);
  my $DI     = extract_bits($blocks[1], 2, 1);
  print_DI($DI_adr, $DI);

  # Traffic Announcements, Music/Speech
  $station{$pi}{TA} = extract_bits($blocks[1], 4, 1);
  $station{$pi}{MS} = extract_bits($blocks[1], 3, 1);
  utter ("  TA:     ".
    $TA_descr[$station{$pi}{TP}][$station{$pi}{TA}],
    " TA:$station{$pi}{TA}");
  utter ("  M/S:    ".qw( Speech Music )[$station{$pi}{MS}],
    " MS:".qw( S M)[$station{$pi}{MS}]);

  $station{$pi}{hasMS} = TRUE;

  if (@blocks == 4) {

    # Program Service name

    if ($station{$pi}{denyPS}) {
      utter ('          (Ignoring changes to PS)', ' denyPS');
    } else {
      set_PS_chars($pi, extract_bits($blocks[1], 0, 2) * 2,
        extract_bits($blocks[3], 8, 8), extract_bits($blocks[3], 0, 8));
      if ($station{$pi}{numPSrcvd} == 4) {
        utter (q{}, ' PS_OK');
      }
    }
  }

}

# 1A: Program Item Number & Slow labeling codes

sub Group1A {

  my @blocks = @_;

  return if (@blocks < 4);

  # Program Item Number

  utter ('  PIN:    '. parse_PIN($blocks[3]),' PIN:'.parse_PIN($blocks[3]));

  # Paging (M.2.1.1.2)

  my $has_paging = FALSE;
  if (extract_bits($blocks[1], 0, 5) != 0) {
    my $has_paging = TRUE;
  }

  if ($has_paging) {
    print_appdata ('Pager', 'TNG: '.     extract_bits($blocks[1], 2, 3));
    print_appdata ('Pager', 'interval: '.extract_bits($blocks[1], 0, 2));
  }

  # Slow labeling codes

  $station{$pi}{LA} = extract_bits($blocks[2], 15, 1);
  utter ('  LA:     '.($station{$pi}{LA} ? 'Program is linked '.
    (exists($station{$pi}{LSN}) &&
    sprintf('to linkage set %Xh ', $station{$pi}{LSN})).
    'at the moment' : 'Program is not linked at the moment'),
    ' LA:'.$station{$pi}{LA}.(exists($station{$pi}{LSN})
    && sprintf('0x%X',$station{$pi}{LSN})));

  my $slc_variant = extract_bits($blocks[2], 12, 3);

  given ($slc_variant) {

    when (0) {
      if ($has_paging) {
        print_appdata ('Pager', 'OPC: '.extract_bits($blocks[2], 8, 4));
      }

      # No PIN, M.3.2.4.3
      if (@blocks == 4 && ($blocks[3] >> 11) == 0) {
        given (extract_bits($blocks[3], 10, 1)) {
          # Sub type 0
          when (0) {
            if ($has_paging) {
              print_appdata ('Pager', 'PAC: '.extract_bits($blocks[3], 4, 6));
              print_appdata ('Pager', 'OPC: '.extract_bits($blocks[3], 0, 4));
            }
          }
          # Sub type 1
          when (1) {
            if ($has_paging) {
              given (extract_bits($blocks[3], 8, 2)) {
                when (0) {
                  print_appdata ('Pager', 'ECC: '.
                    extract_bits($blocks[3], 0, 6));
                }
                when (3) {
                  print_appdata ('Pager', 'CCF: '.
                    extract_bits($blocks[3], 0, 4));
                }
              }
            }
          }
        }
      }

      $station{$pi}{ECC}    = extract_bits($blocks[2],  0, 8);
      $station{$pi}{CC}     = extract_bits($pi,   12, 4);
      utter (('  ECC:    '.sprintf('%02X', $station{$pi}{ECC}).
        (defined $countryISO[$station{$pi}{ECC}][$station{$pi}{CC}] &&
              " ($countryISO[$station{$pi}{ECC}][$station{$pi}{CC}])"),
           (' ECC:'.sprintf('%02X', $station{$pi}{ECC}).
        (defined $countryISO[$station{$pi}{ECC}][$station{$pi}{CC}] &&
              "[$countryISO[$station{$pi}{ECC}][$station{$pi}{CC}]]" ))));
    }

    when (1) {
      $station{$pi}{tmcid}       = extract_bits($blocks[2], 0, 12);
      utter ('  TMC ID: '. sprintf('%xh',$station{$pi}{tmcid}),
        ' TMCID:'.sprintf('%xh',$station{$pi}{tmcid}));
    }

    when (2) {
      if ($has_paging) {
        print_appdata ('Pager', 'OPC: '.extract_bits($blocks[2], 8, 4));
        print_appdata ('Pager', 'PAC: '.extract_bits($blocks[2], 0, 6));
      }

      # No PIN, M.3.2.4.3
      if (@blocks == 4 && ($blocks[3] >> 11) == 0) {
        given (extract_bits($blocks[3], 10, 1)) {
          # Sub type 0
          when (0) {
            if ($has_paging) {
              print_appdata ('Pager', 'PAC: '.extract_bits($blocks[3], 4, 6));
              print_appdata ('Pager', 'OPC: '.extract_bits($blocks[3], 0, 4));
            }
          }
          # Sub type 1
          when (1) {
            given (extract_bits($blocks[3], 8, 2)) {
              if ($has_paging) {
                when (0) {
                  print_appdata ('Pager', 'ECC: '.
                    extract_bits($blocks[3], 0, 6));
                }
                when (3) {
                  print_appdata ('Pager', 'CCF: '.
                    extract_bits($blocks[3], 0, 4));
                }
              }
            }
          }
        }
      }
    }

    when (3) {
      $station{$pi}{lang}        = extract_bits($blocks[2], 0, 8);
      utter ('  Lang:   '. sprintf( ($station{$pi}{lang} <= 127 ?
        "0x%X $langname[$station{$pi}{lang}]" : "Unknown language %Xh"),
        $station{$pi}{lang}),
        ' LANG:'.sprintf( ($station{$pi}{lang} <= 127 ?
        "0x%X[$langname[$station{$pi}{lang}]]" : "%Hx[?]"),
        $station{$pi}{lang}));
    }

    when (6) {
      utter ('  Brodcaster data: '.sprintf('%03x',
        extract_bits($blocks[2], 0, 12)),
        ' BDATA:'.sprintf('%03x', extract_bits($blocks[2], 0, 12)));
    }

    when (7) {
      $station{$pi}{EWS_channel} = extract_bits($blocks[2], 0, 12);
      utter ('  EWS channel: '. sprintf('0x%X',$station{$pi}{EWS_channel}),
             ' EWSch:'. sprintf('0x%X',$station{$pi}{EWS_channel}));
    }

    default {
      say "          SLC variant $slc_variant is not assigned in standard";
    }

  }
}

# 1B: Program Item Number

sub Group1B {
  my @blocks = @_;

  return if (@blocks < 4);

  utter ("  PIN:    ". parse_PIN($blocks[3])," PIN:$blocks[3]");
}

# 2A: RadioText (64 characters)

sub Group2A {

  my @blocks = @_;

  return if (@blocks < 3);

  my $text_seg_addr        = extract_bits($blocks[1], 0, 4) * 4;
  $station{$pi}{prev_textAB} = $station{$pi}{textAB};
  $station{$pi}{textAB}      = extract_bits($blocks[1], 4, 1);
  my @chr                  = ();

  $chr[0] = extract_bits($blocks[2], 8, 8);
  $chr[1] = extract_bits($blocks[2], 0, 8);

  if (@blocks == 4) {
    $chr[2] = extract_bits($blocks[3], 8, 8);
    $chr[3] = extract_bits($blocks[3], 0, 8);
  }

  # Page 26
  if (($station{$pi}{prev_textAB} // -1) != $station{$pi}{textAB}) {
    if ($station{$pi}{denyRTAB} // FALSE) {
      utter ('          (Ignoring A/B flag change)', ' denyRTAB');
    } else {
      utter ('          (A/B flag change; text reset)', ' RT_RESET');
      $station{$pi}{RTbuf}  = q{ } x 64;
      $station{$pi}{RTrcvd} = ();
    }
  }

  set_rt_chars($text_seg_addr, @chr);
}

# 2B: RadioText (32 characters)

sub Group2B {

  my @blocks = @_;

  return if (@blocks < 4);

  my $text_seg_addr            = extract_bits($blocks[1], 0, 4) * 2;
  $station{$pi}{prev_textAB} = $station{$pi}{textAB};
  $station{$pi}{textAB}      = extract_bits($blocks[1], 4, 1);
  my @chr                      = (extract_bits($blocks[3], 8, 8),
                                  extract_bits($blocks[3], 0, 8));

  if (($station{$pi}{prev_textAB} // -1) != $station{$pi}{textAB}) {
    if ($station{$pi}{denyRTAB} // FALSE) {
      utter ('          (Ignoring A/B flag change)', ' denyRTAB');
    } else {
      utter ('          (A/B flag change; text reset)', ' RT_RESET');
      $station{$pi}{RTbuf}  = q{ } x 64;
      $station{$pi}{RTrcvd} = ();
    }
  }

  set_rt_chars($text_seg_addr, @chr);

}

# 3A: Application Identification for Open Data

sub Group3A {

  my @blocks = @_;

  return if (@blocks < 4);

  my $group_type = extract_bits($blocks[1], 0, 5);

  given ($group_type) {

    when (0) {
      utter ('  ODAapp: '.
            ($oda_app[$blocks[3]] // sprintf('0x%04X', $blocks[3])),
             ' ODAapp:'.sprintf('0x%04X',$blocks[3]));
      utter ('          is not carried in associated group','[not_carried]');
      return;
    }

    when (32) {
      utter ('  ODA:    Temporary data fault (Encoder status)',
             ' ODA:enc_err');
      return;
    }

    when ([0..6, 8, 20, 28, 29, 31]) {
      utter ('  ODA:    (Illegal Application Group Type)',' ODA:err');
      return;
    }

    default {
      $station{$pi}{ODAaid}{$group_type} = $blocks[3];
      utter ('  ODAgrp: '. extract_bits($blocks[1], 1, 4).
            (extract_bits($blocks[1], 0, 1) ? 'B' : 'A'),
            ' ODAgrp:'. extract_bits($blocks[1], 1, 4).
            (extract_bits($blocks[1], 0, 1) ? 'B' : 'A'));
      utter ('  ODAapp: '. ($oda_app[$station{$pi}{ODAaid}{$group_type}] //
        sprintf('%04Xh',$station{$pi}{ODAaid}{$group_type})),
        ' ODAapp:'. sprintf('0x%04X',$station{$pi}{ODAaid}{$group_type}));
    }

  }

  given ($station{$pi}{ODAaid}{$group_type}) {

    # Traffic Message Channel
    when ([0xCD46, 0xCD47]) {
      $station{$pi}{hasTMC} = TRUE;
      print_appdata ('TMC', sprintf('sysmsg %04x',$blocks[2]));
    }

    # RT+
    when (0x4BD7) {
      $station{$pi}{hasRTplus} = TRUE;
      $station{$pi}{rtp_which} = extract_bits($blocks[2], 13, 1);
      $station{$pi}{CB}        = extract_bits($blocks[2], 12, 1);
      $station{$pi}{SCB}       = extract_bits($blocks[2],  8, 4);
      $station{$pi}{templnum}  = extract_bits($blocks[2],  0, 8);
      utter ('  RT+ applies to '.($station{$pi}{rtp_which} ?
        'enhanced RadioText' : 'RadioText'), q{});
      utter ('  '.($station{$pi}{CB} ?
        "Using template $station{$pi}{templnum}" : 'No template in use'),
        q{});
      if (!$station{$pi}{CB}) {
        utter (sprintf('  Server Control Bits: %Xh', $station{$pi}{SCB}),
               sprintf(' SCB:%Xh', $station{$pi}{SCB}));
      }
    }

    # eRT
    when (0x6552) {
      $station{$pi}{haseRT}     = TRUE;
      if (not exists $station{$pi}{eRTbuf}) {
        $station{$pi}{eRTbuf}     = q{ } x 64;
      }
      $station{$pi}{ert_isutf8} = extract_bits($blocks[2], 0, 1);
      $station{$pi}{ert_txtdir} = extract_bits($blocks[2], 1, 1);
      $station{$pi}{ert_chrtbl} = extract_bits($blocks[2], 2, 4);
    }

    # Unimplemented ODA
    default {
      say '  ODAmsg: '. sprintf('%04x',$blocks[2]);
      say '          Unimplemented Open Data Application';
    }
  }
}

# 4A: Clock-time and date

sub Group4A {

  my @blocks = @_;

  return if (@blocks < 3);

  my $lto;
  my $mjd = (extract_bits($blocks[1], 0, 2) << 15) |
             extract_bits($blocks[2], 1, 15);

  if (@blocks == 4) {
    # Local time offset
    $lto =  extract_bits($blocks[3], 0, 5) / 2;
    $lto = (extract_bits($blocks[3], 5, 1) ? -$lto : $lto);
    $mjd = int($mjd + $lto / 24);
  }

  my $yr  = int(($mjd - 15078.2) / 365.25);
  my $mo  = int(($mjd - 14956.1 - int($yr * 365.25)) / 30.6001);
  my $dy  = $mjd-14956 - int($yr * 365.25) - int($mo * 30.6001);
  my $k   = ($mo== 14 || $mo == 15);
  $yr += $k + 1900;
  $mo -= 1 + $k * 12;
  #$wd = ($mjd + 2) % 7;

  if (@blocks == 4) {
    my $ltom = ($lto - int($lto)) * 60;
    $lto = int($lto);

    my $hr = ( ( extract_bits($blocks[2], 0, 1) << 4) |
      extract_bits($blocks[3], 12, 4) + $lto) % 24;
    my $mn = extract_bits($blocks[3], 6, 6);

    utter ('  CT:     '. (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13 &&
          $hr > 0 && $hr < 24 && $mn > 0 && $mn < 60) ?
          sprintf('%04d-%02d-%02dT%02d:%02d%+03d:%02d', $yr, $mo, $dy, $hr,
          $mn, $lto, $ltom) : 'Invalid datetime data'),
          " CT:". (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13 && $hr > 0 &&
          $hr < 24 && $mn > 0 && $mn < 60) ?
          sprintf('%04d-%02d-%02dT%02d:%02d%+03d:%02d', $yr, $mo, $dy, $hr,
          $mn, $lto, $ltom) : "err"));
  } else {
    utter ('  CT:     '. (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13) ?
          sprintf('%04d-%02d-%02d', $yr, $mo, $dy) :
          'Invalid datetime data'),
          ' CT:'. (($dy > 0 && $dy < 32 && $mo > 0 && $mo < 13) ?
          sprintf('%04d-%02d-%02d', $yr, $mo, $dy) :
          'err'));
  }

}

# 5A: Transparent data channels or ODA

sub Group5A {

  my @blocks = @_;

  return if (@blocks < 4);

  my $addr = extract_bits($blocks[1], 0, 5);
  my $tds  = sprintf('%02x %02x %02x %02x',
    extract_bits($blocks[2], 8, 8), extract_bits($blocks[2], 0, 8),
    extract_bits($blocks[3], 8, 8), extract_bits($blocks[3], 0, 8));
  utter ('  TDChan: '.$addr, ' TDChan:'.$addr);
  utter ('  TDS:    '.$tds, ' TDS:'.$tds);
}

# 5B: Transparent data channels or ODA

sub Group5B {
  my @blocks = @_;

  return if (@blocks < 4);

  my $addr = extract_bits($blocks[1], 0, 5);
  my $tds  = sprintf('%02x %02x', extract_bits($blocks[3], 8, 8),
    extract_bits($blocks[3], 0, 8));
  utter ('  TDChan: '.$addr, ' TDChan:'.$addr);
  utter ('  TDS:    '.$tds, ' TDS:'.$tds);
}


# 6A: In-House Applications or ODA

sub Group6A {
  my @blocks = @_;

  return if (@blocks < 4);

  my $ih = sprintf('%02x %04x %04x',
    extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]);
  utter ('  InHouse:'.$ih, ' IH:'.$ih);

}

# 6B: In-House Applications or ODA

sub Group6B {
  my @blocks = @_;

  return if (@blocks < 4);
  my $ih = sprintf('%02x %04x', extract_bits($blocks[1], 0, 5), $blocks[3]);
  utter ('  InHouse:'.$ih, ' IH:'.$ih);

}

# 7A: Radio Paging or ODA

sub Group7A {
  my @blocks = @_;

  return if (@blocks < 3);

  print_appdata ('Pager', sprintf('7A: %02x %04x %04x',
    extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]));

}

# 9A: Emergency warning systems or ODA

sub Group9A {
  my @blocks = @_;

  return if (@blocks < 4);

  my $ews = sprintf('%02x %04x %04x',
    extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]);
  utter ('  EWS:    '.$ews, ' EWS:'.$ews);

}

# 10A: Program Type Name (PTYN)

sub Group10A {
  my @blocks = @_;

  if (extract_bits($blocks[1], 4, 1) != ($station{$pi}{PTYNAB} // -1)) {
    utter ('         (A/B flag change, text reset)', q{});
    $station{$pi}{PTYN} = q{ } x 8;
  }

  $station{$pi}{PTYNAB} = extract_bits($blocks[1], 4, 1);

  if (@blocks >= 3) {
    my @char = ();
    $char[0] = extract_bits($blocks[2], 8, 8);
    $char[1] = extract_bits($blocks[2], 0, 8);

    if (@blocks == 4) {
      $char[2] = extract_bits($blocks[3], 8, 8);
      $char[3] = extract_bits($blocks[3], 0, 8);
    }

    my $segaddr = extract_bits($blocks[1], 0, 1);

    for my $cnum (0..$#char) {
      substr($station{$pi}{PTYN}, $segaddr*4 + $cnum, 1,
        $char_table[$char[$cnum]]);
    }

    my $displayed_PTYN
      = ($is_interactive ? '  PTYN:   '.
      substr($station{$pi}{PTYN},0,$segaddr*4).REVERSE.
      substr($station{$pi}{PTYN},$segaddr*4,scalar(@char)).RESET.
      substr($station{$pi}{PTYN},$segaddr*4+scalar(@char)) :
      $station{$pi}{PTYN});
    utter ('  PTYN:   '.$displayed_PTYN, q{ PTYN:"}.$displayed_PTYN.q{"});
  }
}

# 13A: Enhanced Radio Paging or ODA

sub Group13A {
  my @blocks = @_;

  return if (@blocks < 4);

  print_appdata ('Pager', sprintf('13A: %02x %04x %04x',
    extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]));

}

# 14A: Enhanced Other Networks (EON) information

sub Group14A {
  my @blocks = @_;

  return if (@blocks < 4);

  $station{$pi}{hasEON}    = TRUE;
  my $eon_pi                 = $blocks[3];
  $station{$eon_pi}{TP}    = extract_bits($blocks[1], 4, 1);
  my $eon_variant            = extract_bits($blocks[1], 0, 4);
  utter ('  Other Network', ' ON:');
  utter ('    PI:     '.sprintf('%04X',$eon_pi).
    ((exists($station{$eon_pi}{chname})) &&
    " ($station{$eon_pi}{chname})"),
    sprintf("%04X[",$eon_pi));
  utter ('    TP:     '.$TP_descr[$station{$eon_pi}{TP}],
         'TP:'.$station{$eon_pi}{TP});

  given ($eon_variant) {

    when ([0..3]) {
      utter(q{  },q{});
      if (not exists($station{$eon_pi}{PSbuf})) {
        $station{$eon_pi}{PSbuf} = q{ } x 8;
      }
      set_PS_chars($eon_pi, $eon_variant*2, extract_bits($blocks[2], 8, 8),
        extract_bits($blocks[2], 0, 8));
    }

    when (4) {
      utter ('    AF:     '.parse_AF(TRUE, extract_bits($blocks[2], 8, 8)),
             ' AF:'.parse_AF(TRUE, extract_bits($blocks[2], 8, 8)));
      utter ('    AF:     '.parse_AF(TRUE, extract_bits($blocks[2], 0, 8)),
             ' AF:'.parse_AF(TRUE, extract_bits($blocks[2], 0, 8)));
    }

    when ([5..8]) {
      utter('    AF:     Tuned frequency '.
        parse_AF(TRUE, extract_bits($blocks[2], 8, 8)).' maps to '.
        parse_AF(TRUE, extract_bits($blocks[2], 0, 8)),' AF:map:'.
        parse_AF(TRUE, extract_bits($blocks[2], 8, 8)).'->'.
        parse_AF(TRUE, extract_bits($blocks[2], 0, 8)));
    }

    when (9) {
      utter ("    AF:     Tuned frequency ".
        parse_AF(TRUE, extract_bits($blocks[2], 8, 8))." maps to ".
        parse_AF(FALSE,extract_bits($blocks[2], 0, 8)),
        " AF:map:".parse_AF(TRUE, extract_bits($blocks[2], 8, 8))."->".
        parse_AF(FALSE,extract_bits($blocks[2], 0, 8)));
    }

    when (12) {
      $station{$eon_pi}{LA}  = extract_bits($blocks[2], 15,  1);
      $station{$eon_pi}{EG}  = extract_bits($blocks[2], 14,  1);
      $station{$eon_pi}{ILS} = extract_bits($blocks[2], 13,  1);
      $station{$eon_pi}{LSN} = extract_bits($blocks[2], 1,  12);
      if ($station{$eon_pi}{LA})  {
        utter ('    Link: Program is linked to linkage set '.
               sprintf('%03X', $station{$eon_pi}{LSN}),
               ' LSN:'.sprintf('%03X', $station{$eon_pi}{LSN}));
      }
      if ($station{$eon_pi}{EG})  {
        utter ('    Link: Program is member of an extended generic set',
          ' Link:EG');
      }
      if ($station{$eon_pi}{ILS}) {
        utter ('    Link: Program is linked internationally', 'Link:ILS');
      }
      # TODO: Country codes, pg. 51
    }

    when (13) {
      $station{$eon_pi}{PTY} = extract_bits($blocks[2], 11, 5);
      $station{$eon_pi}{TA}  = extract_bits($blocks[2],  0, 1);
      utter (("    PTY:    $station{$eon_pi}{PTY} ".
        (exists $station{$eon_pi}{ECC} &&
        ($countryISO[$station{$pi}{ECC}][$station{$eon_pi}{CC}] // q{})
        =~ /us|ca|mx/ ? $ptynamesUS[$station{$eon_pi}{PTY}] :
        $ptynames[$station{$eon_pi}{PTY}])),
        ' PTY:'.$station{$eon_pi}{PTY});
      utter ('    TA:     '.
        $TA_descr[$station{$eon_pi}{TP}][$station{$eon_pi}{TA}],
        ' TA:'.$station{$eon_pi}{TA});
    }

    when (14) {
      utter ('    PIN:    '.
        parse_PIN($blocks[2]),' PIN:'.parse_PIN($blocks[2]));
    }

    when (15) {
      utter ('    Broadcaster data: '.sprintf('%04x', $blocks[2]),
             ' BDATA:'.sprintf('%04x', $blocks[2]));
    }

    default {
      say "    EON variant $eon_variant is unallocated";
    }

  }
  utter(q{},']');
}

# 14B: Enhanced Other Networks (EON) information

sub Group14B {
  my @blocks = @_;

  return if (@blocks < 4);

  my $eon_pi              =  $blocks[3];
  $station{$eon_pi}{TP} = extract_bits($blocks[1], 4, 1);
  $station{$eon_pi}{TA} = extract_bits($blocks[1], 3, 1);
  utter ('  Other Network', ' ON:');
  utter ('    PI:     '.sprintf('%04X', $eon_pi).
    ((exists($station{$eon_pi}{chname})) &&
    " ($station{$eon_pi}{chname})"),
    sprintf('%04X[',$eon_pi));
  utter ('    TP:     '.
    $TP_descr[$station{$eon_pi}{TP}],
    'TP:'.$station{$eon_pi}{TP});
  utter ('    TA:     '.
    $TA_descr[$station{$eon_pi}{TP}][$station{$eon_pi}{TA}],
    'TA:'.$station{$eon_pi}{TA});
}

# 15B: Fast basic tuning and switching information

sub Group15B {
  my @blocks = @_;

  # DI
  my $DI_adr = 3 - extract_bits($blocks[1], 0, 2);
  my $DI     = extract_bits($blocks[1], 2, 1);
  print_DI($DI_adr, $DI);

  # TA, M/S
  $station{$pi}{TA} = extract_bits($blocks[1], 4, 1);
  $station{$pi}{MS} = extract_bits($blocks[1], 3, 1);
  utter ('  TA:     '.$TA_descr[$station{$pi}{TP}][$station{$pi}{TA}],
         ' TA:'.$station{$pi}{TA});
  utter ('  M/S:    '.qw( Speech Music )[$station{$pi}{MS}],
         ' MS:'.qw(S M)[$station{$pi}{MS}]);
  $station{$pi}{hasMS} = TRUE;

}

# Any group used for Open Data

sub ODAGroup {

  my ($group_type, @blocks) = @_;

  return if (@blocks < 4);

  if (exists $station{$pi}{ODAaid}{$group_type}) {
    given ($station{$pi}{ODAaid}{$group_type}) {

      when ([0xCD46, 0xCD47]) {
        print_appdata ('TMC', sprintf('msg %02x %04x %04x',
          extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]));
      }
      when (0x4BD7) {
        parse_RTp(@blocks);
      }
      when (0x6552) {
        parse_eRT(@blocks);
      }
      default {
        say sprintf('          Unimplemented ODA %04x: %02x %04x %04x',
          $station{$pi}{ODAaid}{$group_type},
          extract_bits($blocks[1], 0, 5), $blocks[2], $blocks[3]);
      }

    }
  } else {
    utter ('          Will need group 3A first to identify ODA', q{});
  }
}

sub screenReset {

  $station{$pi}{RTbuf} = (q{ } x 64) if (!exists $station{$pi}{RTbuf});
  $station{$pi}{hasRT} = FALSE       if (!exists $station{$pi}{hasRT});
  $station{$pi}{hasMS} = FALSE       if (!exists $station{$pi}{hasMS});
  $station{$pi}{TP}    = FALSE       if (!exists $station{$pi}{TP});
  $station{$pi}{TA}    = FALSE       if (!exists $station{$pi}{TA});

}

# Change characters in RadioText

sub set_rt_chars {
  my ($lok, @a) = @_;

  $station{$pi}{hasRT} = TRUE;

  for my $i (0..$#a) {
    given ($a[$i]) {
      when (0x0D) {
        substr($station{$pi}{RTbuf}, $lok+$i, 1, q{↵});
      }
      when (0x0A) {
        substr($station{$pi}{RTbuf}, $lok+$i, 1, q{␊});
      }
      default {
        substr($station{$pi}{RTbuf}, $lok+$i, 1, $char_table[$a[$i]]);
      }
    }
    $station{$pi}{RTrcvd}[$lok+$i] = TRUE;
  }

  my $minRTlen = ($station{$pi}{RTbuf} =~ /↵/ ?
    index($station{$pi}{RTbuf}, q{↵}) + 1 :
    $station{$pi}{presetminRTlen} // 64);

  my $total_received
    = grep { defined } @{$station{$pi}{RTrcvd}}[0..$minRTlen];
  $station{$pi}{hasFullRT} = ($total_received >= $minRTlen ? TRUE : FALSE);

  my $displayed_RT
    = ($is_interactive ? substr($station{$pi}{RTbuf},0,$lok).REVERSE.
                         substr($station{$pi}{RTbuf},$lok,scalar(@a)).RESET.
                         substr($station{$pi}{RTbuf},$lok+scalar(@a)) :
                         $station{$pi}{RTbuf});
  utter ('  RT:     '.$displayed_RT, q{ RT:'}.$displayed_RT.q{'});
  if ($station{$pi}{hasFullRT}) {
    utter (q{}, ' RT_OK');
  }

  utter ('          '. join(q{}, map { defined() ? q{^} : q{ } }
    @{$station{$pi}{RTrcvd}}[0..63]));
}

# Enhanced RadioText

sub parse_eRT {
  my $addr = extract_bits($_[1], 0, 5);

  if ($station{$pi}{ert_chrtbl} == 0x00 &&
     !$station{$pi}{ert_isutf8}         &&
      $station{$pi}{ert_txtdir} == 0) {

    for (0..1) {
      substr($station{$pi}{eRTbuf}, 2*$addr+$_, 1, decode('UCS-2LE',
        chr(extract_bits($_[2+$_], 8, 8)).chr(extract_bits($_[2+$_], 0, 8))));
      $station{$pi}{eRTrcvd}[2*$addr+$_]           = TRUE;
    }

    say '  eRT:    '. substr($station{$pi}{eRTbuf},0,2*$addr).
                      ($is_interactive ? REVERSE : q{}).
                      substr($station{$pi}{eRTbuf},2*$addr,2).
                      ($is_interactive ? RESET : q{}).
                      substr($station{$pi}{eRTbuf},2*$addr+2);

    say '          '. join(q{}, (map { defined() ? q{^} : q{ } }
      @{$station{$pi}{eRTrcvd}}[0..63]));

  }
}

# Change characters in the Program Service name

sub set_PS_chars {
  my ($pspi,$lok,@khar) = @_;

  if (not exists $station{$pspi}{PSbuf}) {
    $station{$pspi}{PSbuf} = q{ } x 8
  }

  substr($station{$pspi}{PSbuf}, $lok, 2,
    $char_table[$khar[0]].$char_table[$khar[1]]);

  # Display PS name when received without gaps

  if (not exists $station{$pspi}{prevPSloc}) {
    $station{$pspi}{prevPSloc} = 0;
  }
  if ($lok != $station{$pspi}{prevPSloc} + 2 ||
      $lok == $station{$pspi}{prevPSloc}) {
    $station{$pspi}{PSrcvd} = ();
  }
  $station{$pspi}{PSrcvd}[$lok/2] = TRUE;
  $station{$pspi}{prevPSloc}      = $lok;
  $station{$pspi}{numPSrcvd}
    = grep { defined } @{$station{$pspi}{PSrcvd}}[0..3];

  my $displayed_PS
    = ($is_interactive ? substr($station{$pspi}{PSbuf},0,$lok).REVERSE.
                         substr($station{$pspi}{PSbuf},$lok,2).RESET.
                         substr($station{$pspi}{PSbuf},$lok+2) :
                         $station{$pspi}{PSbuf});
  utter ('  PS:     '.$displayed_PS, q{ PS:'}.$displayed_PS.q{'});

}

# RadioText+

sub parse_RTp {

  my @ctype;
  my @start;
  my @len;

  # P.5.2
  my $itog  = extract_bits($_[1], 4, 1);
  my $irun  = extract_bits($_[1], 3, 1);
  $ctype[0] = (extract_bits($_[1], 0, 3) << 3) + extract_bits($_[2], 13, 3);
  $ctype[1] = (extract_bits($_[2], 0, 1) << 5) + extract_bits($_[3], 11, 5);
  $start[0] = extract_bits($_[2], 7, 6);
  $start[1] = extract_bits($_[3], 5, 6);
  $len[0]   = extract_bits($_[2], 1, 6);
  $len[1]   = extract_bits($_[3], 0, 5);

  if ($station{$pi}{rtp_which} == 0) {
    my @tags = ();
    for my $tagnum (0..1) {

      my $total_received
        = grep { defined }
        @{$station{$pi}{RTrcvd}}[$start[$tagnum]..($start[$tagnum] +
          $len[$tagnum] - 1)];
      if ($total_received == $len[$tagnum]) {

        my $tagname = $rtpclass[$ctype[$tagnum]];
        my $tagdata = substr($station{$pi}{RTbuf}, $start[$tagnum],
          $len[$tagnum]+1);
        push @tags, $tagname.q{: "}.$tagdata.q{"};
      }
   }
   if (@tags > 0) {
    print_appdata('RadioText+', '{ item.is_running: '.
      ($irun ? 'true' : 'false').', '.join(', ', @tags). ' }');
   }
  } else {
    # (eRT)
  }
}

# Program Item Number

sub parse_PIN {
  my $d = extract_bits($_[0], 11, 5);
  return ($d ? sprintf('%02d@%02d:%02d', $d, extract_bits($_[0], 6, 5),
    extract_bits($_[0], 0, 6)) : 'None');
}

# Decoder Identification

sub print_DI {
  given ($_[0]) {
    when (0) {
      utter ('  DI:     '. qw( Mono Stereo )[$_[1]],
             ' DI:'.qw( Mono Stereo )[$_[1]]);
    }
    when (1) {
      if ($_[1]) {
        utter ('  DI:     Artificial head', ' DI:ArtiHd');
      }
    }
    when (2) {
      if ($_[1]) {
        utter ('  DI:     Compressed', ' DI:Cmprsd');
      }
    }
    when (3) {
      utter ('  DI:     '. qw( Static Dynamic)[$_[1]] .' PTY',
             ' DI:'.qw( StaPTY DynPTY )[$_[1]]);
    }
  }
}

# Alternate Frequencies

sub parse_AF {
  my $is_fm = shift;
  my $num   = shift;
  my $af;
  if ($is_fm) {
    given ($num) {
      when ([1..204]) {
        $af = sprintf('%0.1fMHz', (87.5 + $num / 10) );
      }
      when (205) {
        $af = '(filler)';
      }
      when (224) {
        $af = q{"No AF exists"};
      }
      when ([225..249]) {
        $af = q{"}.($num == 225 ? '1 AF follows' :
          ($num - 224).' AFs follow').q{"};
      }
      when (250) {
        $af = q{"AM/LF freq follows"};
      }
      default {
        $af = '(error:$num)';
      }
    }
  } else {
    given ($num) {
      when ([1..15]) {
        $af = sprintf('%dkHz', 144 + $num * 9);
      }
      when ([16..135]) {
        $af = sprintf('%dkHz', 522 + ($num-15) * 9);
      }
      default {
        $af = 'N/A';
      }
    }
  }
  return $af;
}

sub read_table {

  my ($filename) = @_;

  my @arr;

  $filename = 'tables/'.$filename;
  open my $fh, '<', $filename
    or die "Can't open '$filename'";

  while (<$fh>) {
    chomp();
    if (/^ *(\S+) (.*)/) {
      my ($index, $val) = ($1, $2);
      $index = oct($index) if $index =~ /^0/;
      $arr[$index] = $val;
    }
  }
  close $fh;

  return @arr;
}

sub init_data {

  @ptynames      = read_table('pty_names');
  @langname      = read_table('lang_names');
  @oda_app       = read_table('oda_apps');
  @rtpclass      = read_table('rtplus_classes');
  @ptynamesUS    = read_table('pty_names_us');
  @group_names   = read_table('group_names');

  my @countryECC = read_table('country_iso');
  ECC:
  for my $ECC (0..$#countryECC) {
    next ECC if (not defined $countryECC[$ECC]);
    @{$countryISO[$ECC]} = split(/ /, $countryECC[$ECC]);
  }

  @error_lookup  = read_table('error_vectors');
  VEC:
  for my $vector (@error_lookup) {
    next VEC if (not defined $vector);
    $vector = oct($vector);
  }

  # Basic LCD character set
  @char_table = split(//,
               q{                }.q{                }.
               q{ !"#¤%&'()*+,-./}.q{0123456789:;<=>?}.
               q{@ABCDEFGHIJKLMNO}.q{PQRSTUVWXYZ[\]―_}.
               q{‖abcdefghijklmno}.q[pqrstuvwxyz{|}¯ ].
               q{áàéèíìóòúùÑÇŞβ¡Ĳ}.q{âäêëîïôöûüñçşǧıĳ}.
               q{ªα©‰Ǧěňőπ€£$←↑→↓}.q{º¹²³±İńűµ¿÷°¼½¾§}.
               q{ÁÀÉÈÍÌÓÒÚÙŘČŠŽÐĿ}.q{ÂÄÊËÎÏÔÖÛÜřčšžđŀ}.
               q{ÃÅÆŒŷÝÕØÞŊŔĆŚŹŦð}.q{ãåæœŵýõøþŋŕćśźŧ });

  # Meanings of combinations of TP+TA
  @TP_descr = (  'Does not carry traffic announcements',
                 'Program carries traffic announcements' );
  @TA_descr = ( ['No EON with traffic announcements',
                 'EON specifies another program with traffic announcements'],
                ['No traffic announcement at present',
                 'A traffic announcement is currently being broadcast']);

}

# Extract len bits from int, starting at nth bit from the right
sub extract_bits {
  my ($int, $n, $len) = @_;
  return (($int >> $n) & (2 ** $len - 1));
}

sub print_appdata {
  my ($appname, $data) = @_;
  return if ($is_scanning);

  if (exists $options{t}) {
    my $timestamp = strftime('%Y-%m-%dT%H:%M:%S%z ', localtime);
    print $timestamp;
  }
  say "[app:$appname] $data";
}

sub utter {
  my ($long, $short) = @_;
  return if ($is_scanning);

  if ($verbosity == 0 && defined $short) {
    if ($short =~ /\n/) {
      print $linebuf.$short;
      $linebuf = q{};
    } else {
      $linebuf .= $short;
    }
  } elsif ($verbosity == 1) {
    if ($long ne q{}) {
      print $long."\n";
    }
  }
}

sub log2 {
  return (log($_[0]) / log(2));
}
