#!/usr/bin/perl

# Oona Räisänen (windytan) 2013

use warnings;
$|++;

use constant FALSE => 0;
use constant TRUE  => 1;

use Device::BCM2835;
# Library managment
Device::BCM2835::set_debug(0);
Device::BCM2835::init();

$rdcl0 = $rdda0 = $dataOnRising = FALSE;

# RS R/W DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
# 22  x   15  13   11  16  x   x   x   x

# EN
# 18

while (1) {

  $clockEdge = FALSE;

  $n++;

  $rdda = Device::BCM2835::gpio_lev(RPI_GPIO_P1_12);
  $rdcl = Device::BCM2835::gpio_lev(RPI_GPIO_P1_07);

  $rising  = (!$rdcl0 && $rdcl);
  $falling = ($rdcl0  && !$rdcl);

  # Relevant clock edge
  if (($dataOnRising && $rising) || (!$dataOnRising && $falling)) {
    $clockEdge = TRUE;
  }

  # Data changed on unexpected clock
  #if ($rdda0 != $rdda) {
  #  if (($dataOnRising && $falling) || (!$dataOnRising && $rising)) {
  #    $dataOnRising = !$dataOnRising;
  #    $clockEdge = TRUE;
  #    print STDERR "unexp\n";
  #  }
  #}

  if ($clockEdge) {
    print $rdda;
  }

  $rdda0 = $rdda;
  $rdcl0 = $rdcl;

#  Device::BCM2835::delay(10);
}
