# Redsea tests: End-to-end tests that test the command-line interface.
# These tests aren't supposed to test correctness of the decoding itself in detail,
# but rather that the command-line UI/UX works as expected.
#
# Usage: perl test/end-to-end.pl [--installed] [--skip-sox] [--skip-flac] path/to/redsea.exe
#
# *  --installed: redsea is in PATH (exe file existence won't be checked).
# *  --skip-sox:  Skip tests that require SoX to be installed (PCM input, ...).
# *  --skip-lfs:  Skip tests that require the files in git-lfs to be downloaded.

package redsea::test::end_to_end;

use warnings qw/FATAL all/;
use strict;
use 5.017;
use IPC::Cmd qw/can_run/;
use Carp;
use utf8;
use open qw/:std :utf8/;

my $true  = 1;
my $false = 0;

my $exec_name    = $ARGV[-1] // 'build/redsea';
my $is_installed = ( grep { $_ eq '--installed' } @ARGV ) ? $true : $false;
my $skip_sox     = ( grep { $_ eq '--skip-sox' } @ARGV )  ? $true : $false;
my $skip_lfs     = ( grep { $_ eq '--skip-lfs' } @ARGV )  ? $true : $false;

my $print_even_if_successful = $true;
my $has_failures             = $false;
my $num_skipped_tests        = 0;
my $flac_file                = 'test/resources/rds2-minirds-192k.flac';
my $test_input_file          = '/tmp/redsea-test-input';
my $test_output_file         = '/tmp/redsea-test-output';
my $test_stderr_file         = '/tmp/redsea-test-stderr';

# We can't do 'exit main()'; otherwise the '1;' becomes 'unreachable' (Perl::Critic)
main();

sub main {
    print "Redsea end-to-end tests\n";

    test_Prerequisites();
    exit 1 if ($has_failures);

    test_InputBits();
    test_InputTEF();
    test_InputMPXFile();
    test_InputRawPCM();
    test_IncompatibleOptions();
    test_MildlyIncompatibleOptions();
    test_InvalidOptionArgument();
    test_VersionString();

    print "\n"
      . (
        $has_failures
        ? "\033[31mSome tests failed\033[0m"
        : "\033[32mAll tests passed\033[0m"
      );

    if ( $num_skipped_tests > 0 ) {
        print " (" . "\033[33m$num_skipped_tests skipped" . "\033[0m" . ")";
    }
    print "\n";

    unlink($test_input_file);
    unlink($test_output_file);
    unlink($test_stderr_file);

    exit $has_failures;
}

# Are all the necessary programs installed?
sub test_Prerequisites {
    printTestName("Prerequisites");
    printAssertName($exec_name);
    if ($is_installed) {
        check( can_run($exec_name), $exec_name . ' should be in PATH' );
    }
    else {
        check( -f $exec_name && -x $exec_name,
            $exec_name . ' should be an executable file' );
    }

    printAssertName("sox");
    if ( not skipped( $skip_sox, 'Skipped in this environment' ) ) {
        if ( !check( can_run('sox'), 'SoX should be installed' ) ) {
            print "Note: Use --skip-sox to disable this check.\n";
        }
    }
    printAssertName("lfs files");
    if ( not skipped( $skip_lfs, 'Skipped in this environment' ) ) {
        if (
            !check(
                -f $flac_file && -s $flac_file > 10_000,
                'FLAC file should be downloaded'
            )
          )
        {
            print "Note: Use --skip-lfs to disable this check.\n";
        }
    }

    return;
}

# Can ASCII be fed via stdin?
sub test_InputBits {
    printTestName("ASCII bits input");

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
        printAssertName( 'Option: ' . $arg );
        checkExitSuccess( runRedseaWithArgs( $arg, $test_input_file ) );
        checkStdoutMatches(
'{"pi":"0x22E1","group":"2A","tp":true,"prog_type":"Easy listening"}'
        );
        checkStderrEmpty();
    }

    return;
}

# Can MPX be read from a FLAC file?
sub test_InputMPXFile {
    printTestName("MPX input from FLAC file");

    printAssertName('FLAC');
    return if ( skipped( $skip_lfs, 'Skipped in this environment' ) );

    checkExitSuccess( runRedseaWithArgs("--file $flac_file") );
    checkStdoutMatches('"pi":');
    checkStderrEmpty();

    return;
}

# Can raw PCM be read via stdin? (Requires SoX and git-lfs)
sub test_InputRawPCM {
    printTestName("MPX input from raw PCM file (piped)");

    my $pcm_file_192k = '/tmp/rds2-minirds-192k.raw';
    my $pcm_file_171k = '/tmp/rds2-minirds-171k.raw';

    printAssertName("");

    return if ( skipped( $skip_sox, 'Skipped in this environment' ) );
    return if ( skipped( $skip_lfs, 'Skipped in this environment' ) );

    system(
        "sox $flac_file -r 192k -b 16 -c 1 -e signed-integer $pcm_file_192k");
    check( !$?, 'Created test file 192k' );

    printAssertName("");
    system(
        "sox $flac_file -r 171k -b 16 -c 1 -e signed-integer $pcm_file_171k");
    check( !$?, 'Created test file 171k' );

    printAssertName('Resample from 192k');
    checkExitSuccess( runRedseaWithArgs("--samplerate 192k < $pcm_file_192k") );
    checkStdoutMatches('"pi":');
    checkStderrEmpty();

    printAssertName('Resample from 192000');
    checkExitSuccess(
        runRedseaWithArgs("--samplerate 192000 < $pcm_file_192k") );
    checkStdoutMatches('"pi":');
    checkStderrEmpty();

    # Just typing "redsea" will launch this default function, for
    # backward compatibility. But we have a warning for new users.
    printAssertName('No options 171k');
    checkExitSuccess( runRedseaWithArgs("< $pcm_file_171k") );
    checkStdoutMatches('"pi":');
    checkStderrMatches('warning');

    # Invalid PCM is undetectable. Redsea will just produce no output.
    printAssertName("Invalid raw PCM");

    # Note: using FLAC file as raw PCM input :) It won't work.
    checkExitSuccess(
        runRedseaWithArgs("--samplerate 192k --output hex < $flac_file") );
    checkStdoutEmpty();
    checkStderrEmpty();

    unlink($pcm_file_192k);
    unlink($pcm_file_171k);

    return;
}

# Can TEF6686 serial data be read via stdin?
sub test_InputTEF {
    printTestName("TEF6686 serial input");

    # The TEF6686 serial data in our example set is a little peculiar. Every
    # group is repeated four times, but each time it has fewer and fewer errors.
    # We are replicating this behavior in the test to make it more realistic. We
    # removed everything but RadioText.
    # Data is from https://github.com/windytan/redsea/issues/89

    createTestInputFile(
        "PA201\r\nR2010000000000F\r\nSs65.7,2,35\r\nPA201\r\nR2010D731000003"
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
    printAssertName( 'Option: ' . $arg );
    checkExitSuccess( runRedseaWithArgs( $arg, $test_input_file ) );
    checkStdoutMatches(
        'Ö1 Service: Tel. \\(01\\) 501 70 371 \\(Mo-Fr, 8-21 Uhr\\)');
    checkStderrEmpty();

    return;
}

# Redsea should not start with incompatible options
sub test_IncompatibleOptions {
    printTestName("Incompatible options (fatal)");

    foreach (
        "--streams --input bits",
        "--input hex --samplerate 192000",
        "--input bits --samplerate 192000",
        "--input hex --time-from-start",
        "--input bits --streams",
      )
    {
        printAssertName($_);
        startupShouldFail($_);
    }

    # This would give a false positive if the FLAC file is missing
    if ( !skipped( $skip_lfs, '--input pcm --file' ) ) {
        printAssertName("--input pcm --file");
        startupShouldFail("--input pcm --file $flac_file");
    }

    return;
}

# Redsea can start, but should give a warning
sub test_MildlyIncompatibleOptions {
    printTestName("Incompatible options (non-fatal)");

    printAssertName("--samplerate --file");
    if ( not skipped( $skip_lfs, 'FLAC file not found' ) ) {
        checkExitSuccess(
            runRedseaWithArgs("--file $flac_file --samplerate 192000") );
        checkStderrMatches('warning');
    }

    printAssertName("--output hex --show-raw");
    checkExitSuccess(
        runRedseaWithArgs( "--output hex --show-raw", $test_input_file ) );
    checkStderrMatches('warning');

    printAssertName("--input mpx (no samplerate)");
    checkExitSuccess( runRedseaWithArgs( "--input mpx", $test_input_file ) );
    checkStderrMatches('warning');

    return;
}

sub test_InvalidOptionArgument {
    printTestName("Invalid option argument");

    foreach (
        "--samplerate 192000a",
        "--samplerate -192000",
        "--samplerate 0",
        "--input unknownformat",
        "--output unknownformat"
      )
    {
        printAssertName($_);
        startupShouldFail($_);
    }

    return;
}

# Version, help, invalid options
sub test_VersionString {
    printTestName("Version string, usage help");

    printAssertName("--version");

    checkExitSuccess( runRedseaWithArgs(q{--version}) );
    checkStdoutMatches('^redsea');

    # https://github.com/windytan/redsea/issues/140
    checkStderrEmpty();

    printAssertName("--help");
    checkExitSuccess( runRedseaWithArgs(q{--help}) );
    checkStdoutMatches('^Usage:');
    checkStderrEmpty();

    printAssertName("Invalid option (long)");
    checkExitFailure( runRedseaWithArgs(q{--this-longopt-does-not-exist}) );
    checkStderrMatches('(unrecognized|unknown|invalid) option');
    checkStdoutMatches('^Usage:');

    printAssertName("Invalid option (short)");
    checkExitFailure( runRedseaWithArgs(q{-z}) );
    checkStderrMatches('(unrecognized|unknown|invalid) option');
    checkStdoutMatches('^Usage:');

    return;
}

###
### # Helper functions
###

sub printTestName {
    my ($name) = @_;
    print "\n\n" . $name . "\n" . ( "=" x length($name) ) . "\n";
    return;
}

sub printAssertName {
    my ($name) = @_;
    $name = substr( $name, 0, 24 ) . '...' if ( length($name) > 27 );
    printf "%30s", $name;
    return;
}

sub startupShouldFail {
    my ($args) = @_;
    checkExitFailure( runRedseaWithArgs($args) );
    checkStderrMatches('error');
    checkStdoutEmpty();

    return;
}

# Run the executable for max. 5 seconds
# Returns the exit code, or -1 for timeout
# runRedseaWithArgs( args, input_file ) -> exit_code
sub runRedseaWithArgs {
    my ( $args, $input_file ) = @_;
    my $command =
        $exec_name . q{ }
      . $args
      . ( defined($input_file) ? q{<} . $input_file : q{} ) . q{ >}
      . $test_output_file . q{ 2>}
      . $test_stderr_file;
    my $wait_returned   = 0;
    my $timed_out       = $false;
    my $timeout_seconds = 5;

    unlink($test_output_file);
    unlink($test_stderr_file);

    my $eval_result = eval {

        # Callback on ALRM
        local $SIG{ALRM} = sub { die "timeout\n" };

        # Call it after 5 seconds if we're still here
        alarm $timeout_seconds;
        $wait_returned = system( 'sh', '-c', $command );
        alarm 0;
    };
    if ($@) {
        print "Command timed out: $command\n";
        $timed_out = $true;
        return -1;
    }
    elsif ( $wait_returned == -1 ) {
        print "Failed to run $command: $!\n";
        exit(1);
    }
    elsif ( not defined $eval_result ) {
        print "Failed to run eval\n";
        exit(1);
    }

    # https://perldoc.perl.org/functions/system#system-PROGRAM-LIST
    return $wait_returned >> 8;
}

# bool is expected to be true, otherwise fail with message
sub check {
    my ( $bool, $message ) = @_;
    if ( !$bool || $print_even_if_successful ) {
        print(
            ( $bool ? "  \033[32m[ OK ]\033[0m " : "  \033[31m[FAIL]\033[0m " )
            . $message
              . "\n" );

        $has_failures = $true if ( !$bool );
    }

    return $bool;
}

sub checkStdoutEmpty {
    my $output_empty =
        -e $test_output_file
      ? -z $test_output_file
      : $true;

    printf "%30s", "";
    if ( not check( $output_empty, "stdout should be empty" ) ) {
        previewFileContents($test_output_file);
    }

    return;
}

sub checkStderrEmpty {
    my $stderr_empty =
        -e $test_stderr_file
      ? -z $test_stderr_file
      : $true;

    printf "%30s", "";
    if ( not check( $stderr_empty, "stderr should be empty" ) ) {
        previewFileContents($test_stderr_file);
    }

    return;
}

sub checkFileContentsMatches {
    my ( $file_path, $regex, $test_name ) = @_;
    my $file_exists = -e $file_path;
    my $content;
    if ($file_exists) {
        open( my $file, q{<}, $file_path ) or croak $!;
        $content = do { local $/ = ""; <$file> };
        close $file;
    }

    printf "%30s", "";
    if (
        !(
            check(
                $file_exists
                  && ( ( $content // "" ) =~ $regex ? $true : $false ),
                "$test_name should match /$regex/"
            )
        )
      )
    {
        previewFileContents($file_path);
    }

    return;
}

sub skipped {
    my ( $condition, $message ) = @_;
    if ($condition) {
        print "  \033[33m[SKIP]\033[0m $message\n";
        $num_skipped_tests++;
        return $true;
    }
    return $false;
}

sub checkStderrMatches {
    my ($regex) = @_;

    checkFileContentsMatches( $test_stderr_file, $regex, 'stderr' );

    return;
}

sub checkStdoutMatches {
    my ($regex) = @_;

    checkFileContentsMatches( $test_output_file, $regex, 'stdout' );

    return;
}

sub checkExitSuccess {
    my ($exit_code) = @_;
    check( $exit_code == 0, 'Should exit with EXIT_SUCCESS' );
    return;
}

# Redsea exited with EXIT_FAILURE and did not time out
sub checkExitFailure {
    my ($exit_code) = @_;
    check( $exit_code == 1, 'Should exit with EXIT_FAILURE' );
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

sub previewFileContents {
    my ($file_path) = @_;

    printf "%30s  (contents: \"", "";

    open( my $file, q{<:raw}, $file_path ) or ( print "\") \n" and return );
    local $/ = undef;
    my $contents = <$file>;
    close $file;
    my $count = 0;
    while ( my $char = substr( $contents, $count, 1 ) ) {
        if ( ++$count >= 64 ) {
            print "...";
            last;
        }
        $char =~ s/([^\x20-\x7E])/sprintf("\\x%02X", ord($1))/gex;
        print $char;
    }
    print "\") \n";
    return;
}

1;
