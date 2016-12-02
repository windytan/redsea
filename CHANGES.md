# redsea changelog

## 0.8.1 (2016-12-02)

* Add option `-p` or `--show-partial` to display PS and RadioText before
  completely received (disabled by default)
* Add support for EON variant 4 (alternative frequencies for other networks)
* Add support for LF/MF alternative frequencies
* Add support for frequency quantifiers in TMC
* Change the way alternative frequencies are printed (87.9 becomes "87.9 MHz")
  to allow for LF/MF frequencies
* Fix radiotext decoding for group 2B, thanks Anonymous

## 0.8.0 (2016-11-23)

* Add option `-e` to echo stdin to stdout and print the decoded output to stderr
* Use jsoncpp for JSON serialization
* Change the order of JSON keys (alphabetical instead of PI first) because of
  jsoncpp

## 0.7.6 (2016-10-02)

* Add `partial_radiotext` and `partial_ps` fields that show RadioText and PS
  before they're fully received
* Add support for partially received 15B groups
* Better algorithm for biphase symbol decoding, improves performance with noisy
  signals
* Fix unnecessary sync drops
* Print all received hex blocks even if the beginning of the group was missed
* Try to decode the group even if PI was missed once

## 0.7.5 (2016-09-30)

* Adjust filter parameters to get a slightly better signal in noisy conditions
* Don't correct bit errors longer than 2 bits
* Fix incorrect formatting of negative local time offset with half-hours
* Fix incorrect JSON formatting in TMC Other Network output
* Fix potential JSON error by escaping the backslash character in PS, RT
* Don't print PIN with invalid clock time

## 0.7.4 (2016-09-25)

* Add partial support for 15B groups (Fast basic tuning and switching
  information)
* Add support for "speech/music" flag
* Add support for TMC Other Network info
* Add `./configure` option `--without-liquid` to compile without liquid-dsp and
  demodulation support
* Add `./autogen.sh` to generate `./configure`
* Change TMC disable flag to a `./configure` option as well, `--disable-tmc`
* Change JSON booleans from strings ("true") to plain booleans (true)
* Change JSON format of all TMC location table and service ID numbers from hex
  strings ("0x1F") to plain numbers (31)
* Fix PLL implementation, now actually locks onto a frequency

## 0.7.3 (2016-09-20)

* Compile all TMC text data in to the executable - no external files (#26)
* Add compile-time flag to disable TMC support, as the above can take some time
* Add option `-u` to use North American (RBDS) program type names
* Fix TMC message not showing until the next one is received (#21)
* Fix spelling of program type names (no title case or 16-character limit)
* Fix high error rate in the beginning of reception by increasing AGC bandwidth
* Change format of alternate frequencies from string to numbers
* Change format of TMC location table number from hex string to number

## 0.7.2 (2016-09-18)

* Apply burst error correction to check bits as well
* Fix off-by-one errors in RadioText+ caused by RT characters being converted to
  UTF-8, thanks flux242
* Fix most cases of RadioText+ fields containing segments from a previous RT
  message
* Fix bogus {"pi":"0x0000"} printout at EOF
* Don't print empty or incomplete TMC messages
* Use "debug" JSON object instead of C-style comments for debug information
* Warn if TMC data files couldn't be opened

## 0.7.1 (2016-09-17)

* Fixed broken JSON output in 3A groups, thanks flux242

## 0.7.0 (2016-09-16)

* First numbered release!
