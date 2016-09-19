# redsea changelog

## 0.7.3 (xxxx-xx-xx)

* Compile all TMC text data in to the executable - no external files
* Add option `-u` to use North American (RBDS) program type names
* Fix spelling of program type names (no title case etc)
* Fix high error rate in the beginning of reception by increasing AGC bandwidth
* Print alternate frequencies as numbers instead of strings
* Print TMC location table number as a number instead of hex string

## 0.7.2 (2016-09-18)

* Apply burst error correction to check bits as well
* Fix off-by-one errors in RadioText+ caused by RT characters being converted to UTF-8, thanks flux242
* Fix most cases of RadioText+ fields containing segments from a previous RT message
* Fix bogus {"pi":"0x0000"} printout at EOF
* Don't print empty or incomplete TMC messages
* Use "debug" JSON object instead of C-style comments for debug information
* Warn if TMC data files couldn't be opened

## 0.7.1 (2016-09-17)

* Fixed broken JSON output in 3A groups, thanks flux242

## 0.7.0 (2016-09-16)

* First numbered release!
