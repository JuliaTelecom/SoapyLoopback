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
    _ref_source("internal"),
    time_source("sw_ticks"),
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
    results.push_back("IF4");
    results.push_back("IF5");
    results.push_back("IF6");
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
    if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    {
        int stage = 1;
        if (name.length() > 2)
        {
            int stage_in = name.at(2) - '0';
            if ((stage_in < 1) || (stage_in > 6))
            {
                throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
            }
        }
        IFGain[stage - 1] = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting Loopback IF Gain for stage %d: %f", stage, IFGain[stage - 1]);    }

    if (name == "TUNER")
    {
        tunerGain = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting Loopback Tuner Gain: %f", tunerGain);
    }
}

double SoapyLoopback::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if ((name.length() >= 2) && (name.substr(0, 2) == "IF"))
    {
        int stage = 1;
        if (name.length() > 2)
        {
            int stage_in = name.at(2) - '0';
            if ((stage_in < 1) || (stage_in > 6))
            {
                throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
            } else {
                stage = stage_in;
            }
        }

        return IFGain[stage - 1];
    }

    if (name == "TUNER")
    {
        return tunerGain;
    }

    return 0;
}

SoapySDR::Range SoapyLoopback::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name != "TUNER") {
        if (name == "IF1") {
            return SoapySDR::Range(-3, 6);
        }
        if (name == "IF2" || name == "IF3") {
            return SoapySDR::Range(0, 9);
        }
        if (name == "IF4") {
            return SoapySDR::Range(0, 2);
        }
        if (name == "IF5" || name == "IF6") {
            return SoapySDR::Range(3, 15);
        }
        return SoapySDR::Range(gainMin, gainMax);
    } else {
        return SoapySDR::Range(gainMin, gainMax);
    }
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
    if (name == "RF")
    {
        centerFrequency = frequency;
    } else if (name == "CORR")
    {
        ppm = frequency;
    } else {
        SoapySDR_logf(SOAPY_SDR_ERROR, "RTL-SDR invalid name '%s'", name.c_str());
    }

}

double SoapyLoopback::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return (double) centerFrequency;
    } else if (name == "CORR")
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
    long long ns = SoapySDR::ticksToTimeNs(ticks, sampleRate);
    sampleRate = rate;
    resetBuffer = true;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);
    ticks = SoapySDR::timeNsToTicks(ns, sampleRate);
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
    results.push_back("hw_ticks");

    return results;
}

std::string SoapyLoopback::getTimeSource(void) const
{
    return time_source;
}

void SoapyLoopback::setTimeSource(const std::string &what)
{
    time_source = what;
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
    if (key == "direct_samp")
    {
        try
        {
            directSamplingMode = std::stoi(value);
        }
        catch (const std::invalid_argument &) {
            SoapySDR_logf(SOAPY_SDR_ERROR, "RTL-SDR invalid direct sampling mode '%s', [0:Off, 1:I-ADC, 2:Q-ADC]", value.c_str());
            directSamplingMode = 0;
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode: %d", directSamplingMode);
        //rtlsdr_set_direct_sampling(dev, directSamplingMode);
    }
    else if (key == "iq_swap")
    {
        iqSwap = ((value=="true") ? true : false);
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap ? "true" : "false");
    }
    else if (key == "offset_tune")
    {
        offsetMode = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode ? "true" : "false");
        //rtlsdr_set_offset_tuning(dev, offsetMode ? 1 : 0);
    }
    else if (key == "digital_agc")
    {
        digitalAGC = (value == "true") ? true : false;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR digital agc mode: %s", digitalAGC ? "true" : "false");
        //rtlsdr_set_agc_mode(dev, digitalAGC ? 1 : 0);
    }
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
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}

/*******************************************************************
 * Clocking API
 ******************************************************************/

void SoapyLoopback::setMasterClockRate(const double rate)
{
	//std::unique_lock<std::recursive_mutex> lock(_dev->accessMutex);
	//_ref_clk = rate;
	// xtrx_set_ref_clk(_dev->dev(), _ref_clk, _ref_source);
	// TODO: get reference clock in case of autodetection
}

double SoapyLoopback::getMasterClockRate(void) const
{
	return 0;
	//std::unique_lock<std::recursive_mutex> lock(_dev->accessMutex);
	//int64_t v;

	//int res = xtrx_val_get(_dev->dev(), XTRX_TRX, XTRX_CH_AB, XTRX_REF_REFCLK, &v);
	//if (res)
	//	throw std::runtime_error("SoapyLoopback::getMasterClockRate() unable to get master clock!");

	//return v;
}

SoapySDR::RangeList SoapyLoopback::getMasterClockRates(void) const
{
	SoapySDR::RangeList clks;
	clks.push_back(SoapySDR::Range(0, 0)); // means autodetect
	clks.push_back(SoapySDR::Range(10e6, 52e6));
	return clks;
}

std::vector<std::string> SoapyLoopback::listClockSources(void) const
{
	return { "internal", "extrernal", "ext+pps" };
}

void SoapyLoopback::setClockSource(const std::string &source)
{
    _ref_source = source;
}

std::string SoapyLoopback::getClockSource(void) const
{
    return _ref_source;
}


/*******************************************************************
 * Sensor API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listSensors(void) const
{
	std::vector<std::string> sensors;
	sensors.push_back("clock_locked");
	sensors.push_back("lms7_temp");
	sensors.push_back("board_temp");
	return sensors;
}

SoapySDR::ArgInfo SoapyLoopback::getSensorInfo(const std::string &name) const
{
	SoapySDR::ArgInfo info;
	if (name == "clock_locked")
	{
		info.key = "clock_locked";
		info.name = "Clock Locked";
		info.type = SoapySDR::ArgInfo::BOOL;
		info.value = "false";
		info.description = "CGEN clock is locked, good VCO selection.";
	}
	else if (name == "lms7_temp")
	{
		info.key = "lms7_temp";
		info.name = "LMS7 Temperature";
		info.type = SoapySDR::ArgInfo::FLOAT;
		info.value = "0.0";
		info.units = "C";
		info.description = "The temperature of the LMS7002M in degrees C.";
	}
	else if (name == "board_temp")
	{
		info.key = "board_temp";
		info.name = "XTRX board temerature";
		info.type = SoapySDR::ArgInfo::FLOAT;
		info.value = "0.0";
		info.units = "C";
		info.description = "The temperature of the XTRX board in degrees C.";
	}
	return info;
}

std::string SoapyLoopback::readSensor(const std::string &name) const
{
	if (name == "clock_locked")
	{
		return "true";
	}
	else if (name == "lms7_temp")
	{
		return "1.0";
	}
	else if (name == "board_temp")
	{
		return "1.0";
	}

	throw std::runtime_error("SoapyLoopback::readSensor("+name+") - unknown sensor name");
}

std::vector<std::string> SoapyLoopback::listSensors(const int /*direction*/, const size_t /*channel*/) const
{
	std::vector<std::string> sensors;
	sensors.push_back("lo_locked");
	return sensors;
}

SoapySDR::ArgInfo SoapyLoopback::getSensorInfo(const int /*direction*/, const size_t /*channel*/, const std::string &name) const
{
	SoapySDR::ArgInfo info;
	if (name == "lo_locked")
	{
		info.key = "lo_locked";
		info.name = "LO Locked";
		info.type = SoapySDR::ArgInfo::BOOL;
		info.value = "false";
		info.description = "LO synthesizer is locked, good VCO selection.";
	}
	return info;
}

std::string SoapyLoopback::readSensor(const int /*direction*/, const size_t /*channel*/, const std::string &name) const
{

	if (name == "lo_locked")
	{
		return "true";
	}

	throw std::runtime_error("SoapyLoopback::readSensor("+name+") - unknown sensor name");
}
