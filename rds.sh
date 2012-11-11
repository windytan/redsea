# 1. sox:       Read from device at 48000
# 2. sox:       Filter out FM stereo pilot tone at 19000
# 3. sox:       Bandpass upper sideband of RDS signal
# 4. downmix:   Downmix 19000 -> 0
# 5. sox:       Downsample 48000 -> 6000
# 6. bits:      Demodulate BPSK data bits & apply error-correction
# 7. lcd.pl:    Decode RDS data and display it
sox -q -r 48000 -b 16 -c 2 -t alsa hw:2,0 -c 1 -t raw -b 16 -e signed-integer - \
  sinc -n 4096 -L 19000 \
  sinc 19300-21200 \
  remix 1 | \
./downmix | \
sox -t raw -c 1 -r 48000 -e signed-integer -b 16 - -t raw -r 6000 -e signed-integer -b 16 -c 1 - | \
./bits | \
./lcd.pl
