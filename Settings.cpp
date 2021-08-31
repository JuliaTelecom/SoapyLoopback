/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2015-2017 Josh Blum

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SoapyLoopback.hpp"
#include <SoapySDR/Time.hpp>
#include <algorithm>

SoapyLoopback::SoapyLoopback(const SoapySDR::Kwargs &args):
    deviceId(-1),
    //dev(nullptr),
    //tunerType(RTLSDR_TUNER_R820T),
    sampleRate(2048000),
    centerFrequency(100000000),
    bandwidth(0),
    ppm(0),
    directSamplingMode(0),
    numBuffers(DEFAULT_NUM_BUFFERS),
    bufferLength(DEFAULT_BUFFER_LENGTH),
    iqSwap(false),
    gainMode(false),
    offsetMode(false),
    digitalAGC(false),
    ticks(false),
    bufferedElems(0),
    resetBuffer(false),
    gainMin(0.0),
    gainMax(0.0)
{

}

SoapyLoopback::~SoapyLoopback(void)
{
    //cleanup device handles
    //rtlsdr_close(dev);
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyLoopback::getDriverKey(void) const
{
    return "LoopbackDriver";
}

std::string SoapyLoopback::getHardwareKey(void) const
{
    return "LoopbackHardware";
}

SoapySDR::Kwargs SoapyLoopback::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/juliatelecom/SoapyLoopback";
    args["index"] = "index";

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyLoopback::getNumChannels(const int dir) const
{
    return 2;
}

bool SoapyLoopback::getFullDuplex(const int direction, const size_t channel) const
{
    return false;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> antennas;
    antennas.push_back("RX");
    antennas.push_back("TX");
    return antennas;
}

void SoapyLoopback::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("setAntena failed: RTL-SDR only supports RX");
    }
}

std::string SoapyLoopback::getAntenna(const int direction, const size_t channel) const
{
    return direction ? "RX" : "TX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyLoopback::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return false;
}

bool SoapyLoopback::hasFrequencyCorrection(const int direction, const size_t channel) const
{
    return true;
}

void SoapyLoopback::setFrequencyCorrection(const int direction, const size_t channel, const double value)
{
    ppm = int(value);
}

double SoapyLoopback::getFrequencyCorrection(const int direction, const size_t channel) const
{
    return double(ppm);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IF1");
    results.push_back("IF2");
    results.push_back("IF3");
    results.push_back("TUNER");

    return results;
}

bool SoapyLoopback::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapyLoopback::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    gainMode = automatic;
}

bool SoapyLoopback::getGainMode(const int direction, const size_t channel) const
{
    return gainMode;
}

void SoapyLoopback::setGain(const int direction, const size_t channel, const double value)
{
    //set the overall gain by distributing it across available gain elements
    //OR delete this function to use SoapySDR's default gain distribution algorithm...
    SoapySDR::Device::setGain(direction, channel, value);
}

void SoapyLoopback::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    //if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    //{
    //    int stage = 1;
    //    if (name.length() > 2)
    //    {
    //        int stage_in = name.at(2) - '0';
    //        if ((stage_in < 1) || (stage_in > 6))
    //        {
    //            throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
    //        }
    //    }
    //    if (tunerType == RTLSDR_TUNER_E4000) {
    //        IFGain[stage - 1] = getE4000Gain(stage, (int)value);
    //    } else {
    //        IFGain[stage - 1] = value;
    //    }
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain for stage %d: %f", stage, IFGain[stage - 1]);
    //    rtlsdr_set_tuner_if_gain(dev, stage, (int) IFGain[stage - 1] * 10.0);
    //}
//
    //if (name == "TUNER")
    //{
    //    tunerGain = value;
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR Tuner Gain: %f", tunerGain);
    //    rtlsdr_set_tuner_gain(dev, (int) tunerGain * 10.0);
    //}
}

double SoapyLoopback::getGain(const int direction, const size_t channel, const std::string &name) const
{
    //if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    //{
    //    int stage = 1;
    //    if (name.length() > 2)
    //    {
    //        int stage_in = name.at(2) - '0';
    //        if ((stage_in < 1) || (stage_in > 6))
    //        {
    //            throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
    //        } else {
    //            stage = stage_in;
    //        }
    //    }
    //    if (tunerType == RTLSDR_TUNER_E4000) {
    //        return getE4000Gain(stage, IFGain[stage - 1]);
    //    }
//
    //    return IFGain[stage - 1];
    //}
//
    //if (name == "TUNER")
    //{
    //    return tunerGain;
    //}
//
    //return 0;
}

SoapySDR::Range SoapyLoopback::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    //if (tunerType == RTLSDR_TUNER_E4000 && name != "TUNER") {
    //    if (name == "IF1") {
    //        return SoapySDR::Range(-3, 6);
    //    }
    //    if (name == "IF2" || name == "IF3") {
    //        return SoapySDR::Range(0, 9);
    //    }
    //    if (name == "IF4") {
    //        return SoapySDR::Range(0, 2);
    //    }
    //    if (name == "IF5" || name == "IF6") {
           return SoapySDR::Range(3, 15);
    //    }
//
    //    return SoapySDR::Range(gainMin, gainMax);
    //} else {
    //    return SoapySDR::Range(gainMin, gainMax);
    //}
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyLoopback::setFrequency(
        const int direction,
        const size_t channel,
        const std::string &name,
        const double frequency,
        const SoapySDR::Kwargs &args)
{
    //if (name == "RF")
    //{
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", (uint32_t)frequency);
    //    int r = rtlsdr_set_center_freq(dev, (uint32_t)frequency);
    //    if (r != 0)
    //    {
    //        throw std::runtime_error("setFrequency failed");
    //    }
    //    centerFrequency = rtlsdr_get_center_freq(dev);
    //}
//
    //if (name == "CORR")
    //{
    //    int r = rtlsdr_set_freq_correction(dev, (int)frequency);
    //    if (r == -2)
    //    {
    //        return; // CORR didn't actually change, we are done
    //    }
    //    if (r != 0)
    //    {
    //        throw std::runtime_error("setFrequencyCorrection failed");
    //    }
    //    ppm = rtlsdr_get_freq_correction(dev);
    //}
}

double SoapyLoopback::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return (double) centerFrequency;
    }

    if (name == "CORR")
    {
        return (double) ppm;
    }

    return 0;
}

std::vector<std::string> SoapyLoopback::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    names.push_back("CORR");
    return names;
}

SoapySDR::RangeList SoapyLoopback::getFrequencyRange(
        const int direction,
        const size_t channel,
        const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        results.push_back(SoapySDR::Range(24000000, 1764000000));
    }
    if (name == "CORR")
    {
        results.push_back(SoapySDR::Range(-1000, 1000));
    }
    return results;
}

SoapySDR::ArgInfoList SoapyLoopback::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyLoopback::setSampleRate(const int direction, const size_t channel, const double rate)
{
    //long long ns = SoapySDR::ticksToTimeNs(ticks, sampleRate);
    //sampleRate = rate;
    //resetBuffer = true;
    //SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
    //int r = rtlsdr_set_sample_rate(dev, sampleRate);
    //if (r == -EINVAL)
    //{
    //    throw std::runtime_error("setSampleRate failed: RTL-SDR does not support this sample rate");
    //}
    //if (r != 0)
    //{
    //    throw std::runtime_error("setSampleRate failed");
    //}
    //sampleRate = rtlsdr_get_sample_rate(dev);
    //ticks = SoapySDR::timeNsToTicks(ns, sampleRate);
}

double SoapyLoopback::getSampleRate(const int direction, const size_t channel) const
{
    return sampleRate;
}

std::vector<double> SoapyLoopback::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> results;

    results.push_back(250000);
    results.push_back(1024000);
    results.push_back(1536000);
    results.push_back(1792000);
    results.push_back(1920000);
    results.push_back(2048000);
    results.push_back(2160000);
    results.push_back(2560000);
    results.push_back(2880000);
    results.push_back(3200000);

    return results;
}

SoapySDR::RangeList SoapyLoopback::getSampleRateRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;

    results.push_back(SoapySDR::Range(225001, 300000));
    results.push_back(SoapySDR::Range(900001, 3200000));

    return results;
}

void SoapyLoopback::setBandwidth(const int direction, const size_t channel, const double bw)
{
    bandwidth = bw;
}

double SoapyLoopback::getBandwidth(const int direction, const size_t channel) const
{
    if (bandwidth == 0) // auto / full bandwidth
        return sampleRate;
    return bandwidth;
}

std::vector<double> SoapyLoopback::listBandwidths(const int direction, const size_t channel) const
{
    std::vector<double> results;

    return results;
}

SoapySDR::RangeList SoapyLoopback::getBandwidthRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;

    // stub, not sure what the sensible ranges for different tuners are.
    results.push_back(SoapySDR::Range(0, 8000000));

    return results;
}

/*******************************************************************
 * Time API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listTimeSources(void) const
{
    std::vector<std::string> results;

    results.push_back("sw_ticks");

    return results;
}

std::string SoapyLoopback::getTimeSource(void) const
{
    return "sw_ticks";
}

bool SoapyLoopback::hasHardwareTime(const std::string &what) const
{
    return what == "" || what == "sw_ticks";
}

long long SoapyLoopback::getHardwareTime(const std::string &what) const
{
    return SoapySDR::ticksToTimeNs(ticks, sampleRate);
}

void SoapyLoopback::setHardwareTime(const long long timeNs, const std::string &what)
{
    ticks = SoapySDR::timeNsToTicks(timeNs, sampleRate);
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyLoopback::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;

    SoapySDR::ArgInfo directSampArg;

    directSampArg.key = "direct_samp";
    directSampArg.value = "0";
    directSampArg.name = "Direct Sampling";
    directSampArg.description = "RTL-SDR Direct Sampling Mode";
    directSampArg.type = SoapySDR::ArgInfo::STRING;
    directSampArg.options.push_back("0");
    directSampArg.optionNames.push_back("Off");
    directSampArg.options.push_back("1");
    directSampArg.optionNames.push_back("I-ADC");
    directSampArg.options.push_back("2");
    directSampArg.optionNames.push_back("Q-ADC");

    setArgs.push_back(directSampArg);

    SoapySDR::ArgInfo offsetTuneArg;

    offsetTuneArg.key = "offset_tune";
    offsetTuneArg.value = "false";
    offsetTuneArg.name = "Offset Tune";
    offsetTuneArg.description = "RTL-SDR Offset Tuning Mode";
    offsetTuneArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(offsetTuneArg);

    SoapySDR::ArgInfo iqSwapArg;

    iqSwapArg.key = "iq_swap";
    iqSwapArg.value = "false";
    iqSwapArg.name = "I/Q Swap";
    iqSwapArg.description = "RTL-SDR I/Q Swap Mode";
    iqSwapArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(iqSwapArg);

    SoapySDR::ArgInfo digitalAGCArg;

    digitalAGCArg.key = "digital_agc";
    digitalAGCArg.value = "false";
    digitalAGCArg.name = "Digital AGC";
    digitalAGCArg.description = "RTL-SDR digital AGC Mode";
    digitalAGCArg.type = SoapySDR::ArgInfo::BOOL;

    setArgs.push_back(digitalAGCArg);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "SETARGS?");

    return setArgs;
}

void SoapyLoopback::writeSetting(const std::string &key, const std::string &value)
{
    //if (key == "direct_samp")
    //{
    //    try
    //    {
    //        directSamplingMode = std::stoi(value);
    //    }
    //    catch (const std::invalid_argument &) {
    //        SoapySDR_logf(SOAPY_SDR_ERROR, "RTL-SDR invalid direct sampling mode '%s', [0:Off, 1:I-ADC, 2:Q-ADC]", value.c_str());
    //        directSamplingMode = 0;
    //    }
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode: %d", directSamplingMode);
    //    rtlsdr_set_direct_sampling(dev, directSamplingMode);
    //}
    //else if (key == "iq_swap")
    //{
    //    iqSwap = ((value=="true") ? true : false);
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap ? "true" : "false");
    //}
    //else if (key == "offset_tune")
    //{
    //    offsetMode = (value == "true") ? true : false;
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode ? "true" : "false");
    //    rtlsdr_set_offset_tuning(dev, offsetMode ? 1 : 0);
    //}
    //else if (key == "digital_agc")
    //{
    //    digitalAGC = (value == "true") ? true : false;
    //    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR digital agc mode: %s", digitalAGC ? "true" : "false");
    //    rtlsdr_set_agc_mode(dev, digitalAGC ? 1 : 0);
    //}
}

std::string SoapyLoopback::readSetting(const std::string &key) const
{
    if (key == "direct_samp") {
        return std::to_string(directSamplingMode);
    } else if (key == "iq_swap") {
        return iqSwap?"true":"false";
    } else if (key == "offset_tune") {
        return offsetMode?"true":"false";
    } else if (key == "digital_agc") {
        return digitalAGC?"true":"false";
#if HAS_RTLSDR_SET_BIAS_TEE
    } else if (key == "biastee") {
        return biasTee?"true":"false";
#endif
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}
