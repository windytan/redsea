# Basic tests to see if the CLI works at all. Correctness is tested (mostly) elsewhere.
# Usage: perl cli.pl path/to/redsea.exe [--installed]

package cli;

use warnings;
use strict;
use IPC::Cmd qw/can_run/;
use Carp;
use utf8;
use open qw/:std :utf8/;

my $exec_name                = $ARGV[0] // 'build/redsea';
my $is_installed             = ( $ARGV[1] // "" ) eq '--installed';
my $print_even_if_successful = 1;
my $has_failures             = 0;
my $test_input_file          = 'redsea-test-input';
my $test_output_file         = 'redsea-test-output';

main();

sub main {
  testExeRunnable();
  testInputBits();
  testInputTEF();

  exit $has_failures;
}

sub testExeRunnable {
  req( $is_installed ? can_run($exec_name) : -f $exec_name && -x $exec_name,
    'Executable is found' );

  return;
}

sub testInputBits {
  createTestInputFile( "001\n"
      . "1110110110111010011100010101001000010100001110000010\n"
      . "0010001011100001011100110000100101100000111100111110\n"
      . "0010000001100101101101001101101001001000000110111110\n"
      . "0010001011100001011100110000000101100010010011100000\n"
      . "1010011010110011111010010101010011010011000101010101\n"
      . "0010001011100001011100110000100101100001001010101000\n"
      . "0111001101100001010000011001100001000011010111000111\n"
      . "001000" );

  for my $arg ( '-b', '--input bits', '--input-bits' ) {
    PrintTestName( 'Option: ' . $arg );
    unlink($test_output_file);
    RunRedseaWithArgs( $arg . q{<} . $test_input_file . q{>} . $test_output_file );
    open( my $test_output, q{<}, $test_output_file ) or croak $!;
    my $result = (
      index(
        <$test_output>, '{"pi":"0x22E1","group":"2A","tp":true,"prog_type":"Easy listening"}'
      ) != -1
    );
    close $test_output;
    check( $result, 'decodes ASCII binary' );
  }

  return;
}

# The TEF6686 serial data in our example set is a little peculiar. Every group is repeated
# four times, but each time it has fewer and fewer errors. We are replicating this behavior
# in the test to make it more realistic. We removed everything but RadioText.
# Data is from https://github.com/windytan/redsea/issues/89
sub testInputTEF {
  createTestInputFile( "PA201\r\nR2010000000000F\r\nSs65.7,2,35\r\nPA201\r\nR2010D731000003"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR2010D731205300\r\nSs65.7,2,34\r\nPA201\r\nR2011000000000F"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR20116572000003\r\nSs65.7,2,35\r\nPA201\r\nR20116572766900"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR2012000000000F\r\nSs65.7,2,35\r\nPA201\r\nR20126365000003"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR201263653A2000\r\nSs65.7,2,35\r\nPA201\r\nR2013000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR20135465000003\r\nSs65.7,2,34\r\nPA201\r\nR201354656C2E00"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR2014000000000F\r\nSs65.7,2,35\r\nPA201\r\nR20142028000003"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR20142028303100\r\nSs65.7,2,35\r\nPA201\r\nR2015000000000F"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR20152920000003\r\nSs65.7,2,35\r\nPA201\r\nR20152920353000"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR2016000000000F\r\nSs65.7,2,35\r\nPA201\r\nR20163120000003"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR20163120373000\r\nSs65.7,2,34\r\nPA201\r\nR2017000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR20172033000003\r\nSs65.7,2,34\r\nPA201\r\nR20172033000003"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR20172033373100\r\nSs65.7,2,35\r\nPA201\r\nR2018000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR20182028000003\r\nSs65.7,2,34\r\nPA201\r\nR201820284D6F00"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR2019000000000F\r\nSs65.7,2,34\r\nPA201\r\nR20192D46000003"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR20192D46722C00\r\nSs65.7,2,34\r\nPA201\r\nR201A000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201A2038000003\r\nSs65.7,2,34\r\nPA201\r\nR201A20382D3200"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR201B000000000F\r\nSs65.7,2,35\r\nPA201\r\nR201B3120000003"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201B3120556800\r\nSs65.7,2,34\r\nPA201\r\nR201C000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201C7229000003\r\nSs65.7,2,34\r\nPA201\r\nR201C7229202000"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR201D000000000F\r\nSs65.7,2,34\r\nPA201\r\nR201D2020000003"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201D2020202000\r\nSs65.7,2,34\r\nPA201\r\nR201E000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201E2020000003\r\nSs65.7,2,34\r\nPA201\r\nR201E2020000003"
      . "\r\nSs65.7,2,35\r\nPA201\r\nR201E2020202000\r\nSs65.7,2,35\r\nPA201\r\nR201F000000000F"
      . "\r\nSs65.7,2,34\r\nPA201\r\nR201F2020000003\r\nSs65.7,2,34\r\nPA201\r\nR201F2020202000"
      . "\r\nSs65.7,2,34\r\n" );

  my $arg = "--input tef";
  PrintTestName( 'Option: ' . $arg );
  unlink($test_output_file);
  RunRedseaWithArgs( $arg . q{<} . $test_input_file . q{| grep radiotext >} . $test_output_file );
  open( my $test_output, q{<}, $test_output_file ) or croak $!;
  my $result =
    ( index( <$test_output>, 'Ö1 Service: Tel. (01) 501 70 371 (Mo-Fr, 8-21 Uhr)' ) != -1 );
  close $test_output;
  check( $result, 'decodes TEF6686 serial output' );

  return;
}

# Just print the test name
sub PrintTestName {
  my ($name) = @_;
  print $name. q{ };

  return;
}

# Run the executable
sub RunRedseaWithArgs {
  my ($args) = @_;
  my $command = $exec_name . q{ } . $args;
  system( 'sh', '-c', $command );

  return;
}

# bool is expected to be true, otherwise fail with message
sub check {
  my ( $bool, $message ) = @_;
  if ( !$bool || $print_even_if_successful ) {
    print( ( $bool ? '[ OK ] ' : '[FAIL] ' ) . $message . "\n" );

    $has_failures = 1 if ( !$bool );
  }

  return;
}

# bool is expected to be true, otherwise fail with message + exit
sub req {
  my ( $bool, $message ) = @_;
  if ( !$bool || $print_even_if_successful ) {
    print( ( $bool ? '[ OK ] ' : '[FAIL] ' ) . $message . "\n" );

    exit(1) if ( !$bool );
  }

  return;
}

# Create a file with $contents
sub createTestInputFile {
  my ($contents) = @_;
  unlink($test_input_file);
  open my $file, q{>}, $test_input_file or croak $!;
  print $file $contents;
  close $file;

  return;
}

1;
