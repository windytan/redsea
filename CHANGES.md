# redsea changelog

## 0.20

* Add support for fractional seconds in the rx timestamp format (`%f`).
* Add buffer delay compensation to rx timestamps.
* Place some JSON fields in the beginning of the line for easier visual
  inspection (pi, group, ps...)

## 0.19 (2020-04-06)

* Add option `--show-raw` (`-R`) for including the raw group data as a hex
  string in the JSON output
* Print usage help if there are non-option arguments on the command line
* Fix uninitialized block error rate values on some systems
* Update jsoncpp from 1.8.1 to 1.8.4 to fix some warnings

## 0.18 (2019-05-19)

* Add support for loading multiple TMC location databases by specifying
  `--loctable` more than once.
* Sample rate can also be specified as `-r 171k` instead of `-r 171000`.
* Fix a crash if the input audio file couldn't be loaded.
* Print usage help instead of error message when stdin is empty.
* Speed up loading of TMC location database.
* Improve block sync detection by ignoring spurious sync pulses.
* Fine-tune filter bandwidths for better sensitivity, based on test runs.
* At EOF, process the last partially received group.
* Remove character codetables G1 and G2 since they don't appear in the latest
  RDS standard any more.
* Clean up code to ensure maintainability. Redsea now requires a compiler
  that supports C++14.

## 0.17.1 (2018-06-08)

* Fix: Return exit value 0 if `--version` or `--help` was requested
* Don't open stdout for libsndfile unless `--feed-through` was specified,
  otherwise json can't be written (this may break the feed-through on Linux
  temporarily)

## 0.17.0 (2018-06-05)

* Change the type of the JSON field for TMC message urgency, from integer
  (0, 1, 2) to string ("none", "U", "X")
* Speed improvements by avoiding a few extraneous buffer copies internally

## 0.16.0 (2018-03-27)

* Add support for multi-channel signals (`--channels`) - libsndfile is now
  a required dependency
* Speed improvements:
  * By using a lookup table instead of sinf/cosf to generate the
    mix-down sinusoid
  * By only using one mix-down operation instead of two

## 0.15.0 (2018-01-31)

* Add `prog_item_number` field containing the raw Program Item Number
* Add `partial_alt_kilohertz` field containing an incomplete list of
  alternative frequencies when the `--show-partial` option is used
* Add configure option `--without-macports` to disable directory checks
  when cross-compiling
* Reduce write calls to output JSON stream using a stringstream (#56)
* Remove an old shell script that launches `rtl_fm` - instead, there is
  a mention of `rtl_fm` usage in the readme and wiki

## 0.14.1 (2017-12-08)

* Change the `callsign` object name to `callsign_uncertain` when a station's PI
  code begins with a 1
* Fix UTC+14 getting marked as invalid (it's the time zone in Kiribati)
* Fix syntax errors in schema

## 0.14.0 (2017-11-14)

* Add support for decoding call sign letters from North American (RBDS)
  stations - activated by the `-u` switch

## 0.13.0 (2017-09-19)

* Change the JSON structure in `radiotext_plus`: RT+ tags are now displayed as
  an array of objects with content-type and data:
```json
"radiotext_plus":{
  "tags":[
    {"content-type":"item.artist",
     "data":"SeeB feat. Neev"},
    {"content-type":"item.title",
     "data":"Breathe"}
  ]
}
```
* Change the way frequencies are displayed for better machine readability, from
  `"frequency":"87.9 MHz"` to `"kilohertz":87900`
* Update jsoncpp from version 1.7.7 to 1.8.1

## 0.12.0 (2017-07-22)

* Add support for time-of-demodulation timestamps (`-t`, `--timestamp`)
* Add support for average block error rate (BLER) estimation (`-E`, `--bler`)
* Change the long option for `-b` to `--input-bits` and fix the incorrect
  option in the usage help

## 0.11.0 (2017-04-17)

* Add support for TMC alternative frequencies (tuning info variant 6)
* Add support for TMC gap parameter and enhanced mode (3A group variant 1)
* Change JSON schema for TMC: `encryption_id` is now under `system_info`
* Change PLL bandwidth and lowpass cutoff frequency to improve noise performance
* Change resampler anti-alias cutoff frequency to allow for lower sample rates
* Fix missing zero-padding in PIN time string
* Fix uninitialized PI field occasionally getting printed when the actual PI
  is missed
* Fix some names not getting read properly from the location database
* Fix grammar in some TMC event descriptions
* Fix potentially uninitialized data printout when a type 6B group was not fully
  received
* Fix potentially uninitialized RadioText+ tags when the group was not fully
  received
* Fix spurious printouts of "version B" groups when the C' offset was not seen

## 0.10.0 (2017-04-03)

* Add support for non-default sample rates (`-r` option) using an internal
  resampler
* Add support for reading audio files (`libsndfile` dependency, can be disabled)

## 0.9.2 (2017-03-30)

* Fix location table info being sometimes printed into the wrong stream
* Fix conflicting compiler optimization flags

## 0.9.1 (2017-02-11)

* Add support for type 14B EON groups
* Change rtl-rx.sh to run redsea installed in $PATH

## 0.9.0 (2016-12-15)

* Add support for Decoder Identification (DI)
* Add support for auxiliary character code tables (G1 and G2)
* Add support for TMC location tables (`--loctable DIR`), adds iconv dependency
* Add schema to specify JSON output format
* Fix UTC time zone being displayed as "-00:00"
* Fix PS strings getting trimmed

## 0.8.1 (2016-12-02)

* Add option `-p` or `--show-partial` to display PS and RadioText before
  completely received (disabled by default)
* Add support for EON variant 4 (alternative frequencies for other networks)
* Add support for LF/MF alternative frequencies
* Add support for frequency quantifiers in TMC
* Change the way alternative frequencies are printed (87.9 becomes "87.9 MHz")
  to allow for LF/MF frequencies
* Fix radiotext decoding for group 2B, thanks Anonymous
* Fix empty hex group printout at EOF

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
