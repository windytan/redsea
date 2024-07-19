# Basic tests to see if the CLI works at all. Correctness is tested elsewhere.
# Usage: perl cli.pl path/to/redsea.exe [--installed]

use warnings;
use strict;
use IPC::Cmd qw(can_run);
use Carp;

my $exec_name                = $ARGV[0] // 'build/redsea';
my $is_installed             = ( $ARGV[1] // "" ) eq '--installed';
my $print_even_if_successful = 1;
my $has_failures             = 0;
my $test_input_file          = 'redsea-test-input';
my $test_output_file         = 'redsea-test-output';

exit main();

sub main {
  testExeRunnable();
  testInputBits();

  return $has_failures;
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
    TestName( 'Option: ' . $arg );
    unlink($test_output_file);
    runWithArgs( $arg . q{<} . $test_input_file . q{>} . $test_output_file );
    open( my $test_output, q{<}, $test_output_file ) or croak $!;
    my $result =
      ( ( <$test_output> // "" ) =~
        /\{"pi":"0x22E1","group":"2A","tp":true,"prog_type":"Easy listening"\}/ );
    close $test_output;
    check( $result, 'decodes ASCII binary' );
  }

  return;
}

# Just print the test name
sub TestName {
  my ($name) = @_;
  print $name. q{ };

  return;
}

# Run the executable
sub runWithArgs {
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
