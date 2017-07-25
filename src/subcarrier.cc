/*
 * Copyright (c) Oona Räisänen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include "src/subcarrier.h"

#ifdef HAVE_LIQUID

#include <cmath>
#include <complex>
#include <cstdio>
#include <deque>
#include <iostream>
#include <tuple>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"

namespace redsea {

namespace {

const float kCarrierFrequency_Hz  = 57000.0f;
const float kBitsPerSecond        = 1187.5f;
const int   kSamplesPerSymbol     = 3;
const float kAGCBandwidth_Hz      = 500.0f;
const float kAGCInitialGain       = 0.0077f;
const float kLowpassCutoff_Hz     = 2600.0f;
const float kSymsyncBandwidth_Hz  = 2400.0f;
const int   kSymsyncDelay         = 2;
const float kSymsyncBeta          = 0.8f;
const float kPLLBandwidth_Hz      = 0.01f;
const float kPLLMultiplier        = 12.0f;

float hertz2step(float Hz) {
  return Hz * 2.0f * M_PI / kTargetSampleRate_Hz;
}

#ifdef DEBUG
float step2hertz(float step) {
  return step * kTargetSampleRate_Hz / (2.0f * M_PI);
}
#endif

}  // namespace

BiphaseDecoder::BiphaseDecoder() : prev_psk_symbol_(0.0f),
  clock_history_(48), clock_(0), clock_polarity_(0) {
}

BiphaseDecoder::~BiphaseDecoder() {
}

// Return {is_clock, symbol}
//   is_clock: true if symbol valid
//   symbol:   binary symbol in constellation {-1,0} => 0, {1,0} => 1
std::pair<bool, std::complex<float>> BiphaseDecoder::push(
    const std::complex<float>& psk_symbol) {

  std::complex<float> biphase = (psk_symbol - prev_psk_symbol_) * 0.5f;
  bool is_clock = (clock_ % 2 == clock_polarity_);

  clock_history_[clock_] = std::fabs(biphase.real());

  // Periodically evaluate validity of the chosen biphase clock polarity
  if (++clock_ == clock_history_.size()) {
    float a = 0;
    float b = 0;

    for (size_t i = 0; i < clock_history_.size(); i++) {
      if (i % 2 == 0)
        a += clock_history_[i];
      else
        b += clock_history_[i];
      clock_history_[i] = 0.f;
    }

    if      (a > b) clock_polarity_ = 0;
    else if (b > a) clock_polarity_ = 1;

    clock_ = 0;
  }

  prev_psk_symbol_ = psk_symbol;

  return {is_clock, biphase};
}

DeltaDecoder::DeltaDecoder() : prev_(0) {
}

DeltaDecoder::~DeltaDecoder() {
}

unsigned DeltaDecoder::Decode(unsigned d) {
  unsigned bit = (d != prev_);
  prev_ = d;
  return bit;
}

Subcarrier::Subcarrier(const Options& options) : sample_num_(0),
    resample_ratio_(kTargetSampleRate_Hz / options.samplerate),
    bit_buffer_(),
    fir_lpf_(256, kLowpassCutoff_Hz / kTargetSampleRate_Hz),
    agc_(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain),
    oscillator_approx_(LIQUID_VCO, hertz2step(kCarrierFrequency_Hz)),
    oscillator_exact_(LIQUID_VCO, hertz2step(kCarrierFrequency_Hz)),
    symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay,
             kSymsyncBeta, 32),
    modem_(LIQUID_MODEM_PSK2),
    resampler_(resample_ratio_, 13),
    is_eof_(false), delta_decoder_() {
  symsync_.set_bandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
  symsync_.set_output_rate(1);
  oscillator_exact_.set_pll_bandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);

#ifdef HAVE_SNDFILE
  if (options.input_type == INPUT_MPX_SNDFILE)
    mpx_ = new SndfileReader(options);
  else
#endif
    mpx_ = new StdinReader(options);

  resample_ratio_ = kTargetSampleRate_Hz / mpx_->samplerate();
  resampler_.set_rate(resample_ratio_);
}

Subcarrier::~Subcarrier() {
}

/** MPX to bits
 */
void Subcarrier::DemodulateMoreBits() {
  // Read from MPX source
  is_eof_ = mpx_->eof();
  if (is_eof_)
    return;

  std::vector<float> inbuffer = mpx_->ReadBlock();

  // Resample if needed
  int num_samples = 0;

  std::vector<std::complex<float>> complex_samples(
      resample_ratio_ <= 1.0f ? inbuffer.size() :
                                inbuffer.size() * resample_ratio_);

  if (resample_ratio_ == 1.0f) {
    for (size_t i = 0; i < inbuffer.size(); i++)
      complex_samples[i] = inbuffer[i];
    num_samples = inbuffer.size();
  } else {
    int i_resampled = 0;
    for (size_t i = 0; i < inbuffer.size(); i++) {
      std::complex<float> buf[4];
      int num_resampled = resampler_.execute(inbuffer[i], buf);

      for (int j = 0; j < num_resampled; j++) {
        complex_samples[i_resampled] = buf[j];
        i_resampled++;
      }
    }
    num_samples = i_resampled;
  }

  const int decimate_ratio = kTargetSampleRate_Hz / kBitsPerSecond / 2 /
                             kSamplesPerSymbol;

  for (int i = 0; i < num_samples; i++) {
    std::complex<float> sample = complex_samples[i];

    // Mix RDS to baseband for filtering purposes
    std::complex<float> sample_baseband = oscillator_approx_.MixDown(sample);

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % decimate_ratio == 0) {
      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      // PLL-controlled 57 kHz mixdown - aliasing is intentional so we don't
      // have to mix it back up first
      sample_lopass = oscillator_exact_.MixDown(sample_lopass);

      std::vector<std::complex<float>> symbols =
        symsync_.execute(&sample_lopass);

      for (std::complex<float> symbol : symbols) {
#ifdef DEBUG
        printf("sy:%f,%f,%f\n",
            sample_num_ / kTargetSampleRate_Hz,
            symbol.real(),
            symbol.imag());
#endif

        // Modem here is only used to track PLL phase error
        modem_.Demodulate(symbol);
        oscillator_exact_.StepPLL(modem_.phase_error() * kPLLMultiplier);

        bool is_clock;
        std::complex<float> biphase;
        std::tie(is_clock, biphase) = biphase_decoder_.push(symbol);

        // One biphase symbol received for every 2 PSK symbols
        if (is_clock) {
          bit_buffer_.push_back(delta_decoder_.Decode(
                biphase.real() >= 0));
#ifdef DEBUG
          printf("bi:%f,%f,%f\n",
              sample_num_ / kTargetSampleRate_Hz,
              biphase.real(),
              biphase.imag());
#endif
        }
      }
#ifdef DEBUG
      printf("f:%f,%f,%f,%f,%f,%f,%f\n",
          sample_num_ / kTargetSampleRate_Hz,
          static_cast<float>(sample),
          step2hertz(oscillator_exact_.frequency()),
          modem_.phase_error(),
          agc_.gain(),
          sample_lopass.real(),
          sample_lopass.imag());
#endif
    }

    oscillator_exact_.Step();
    oscillator_approx_.Step();

    sample_num_++;
  }
}

int Subcarrier::NextBit() {
  while (bit_buffer_.size() < 1 && !eof())
    DemodulateMoreBits();

  int bit = 0;

  if (bit_buffer_.size() > 0) {
    bit = bit_buffer_.front();
    bit_buffer_.pop_front();
  }

  return bit;
}

bool Subcarrier::eof() const {
  return is_eof_;
}

#ifdef DEBUG
float Subcarrier::t() const {
  return sample_num_ / kTargetSampleRate_Hz;
}
#endif

}  // namespace redsea

#endif  // HAVE_LIQUID
