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
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Registry.hpp>
#include <algorithm>

SoapyLoopback::SoapyLoopback(const SoapySDR::Kwargs & /*args*/) :
    numChannels(DEFAULT_NUM_CHANNELS),
    numAntennas(DEFAULT_NUM_ANTENNAS),
    numGains(DEFAULT_NUM_GAINS),
    sampleFormat(SOAPY_SDR_CF32),
    clockSource("internal"),
    frequency(1e9),
    sampleRate(10e6),
    bandwidth(sampleRate),
    masterClockRate(10e6),
    iqSwap(false),
    ringBuff(NULL),
    readAdapter(NULL),
    writeAdapter(NULL)
{
    // Set the initial antenna selections for each direction and each channel.
    this->antennaSelections.resize(2);
    for (int dir_idx=0;dir_idx<2; dir_idx++) {
        this->antennaSelections[dir_idx].resize(this->numChannels);
        std::string prefix = dir_idx == SOAPY_SDR_RX ? "RX" : "TX";
        for (size_t chan_idx=0; chan_idx<this->numChannels; ++chan_idx) {
            this->antennaSelections[dir_idx][chan_idx] = prefix + "0";
        }
    }

    // Set the initial gains/gain modes for each direction and each channel
    this->gains.resize(2);
    this->gainModes.resize(2);
    for (int dir_idx=0;dir_idx<2; dir_idx++) {
        this->gains[dir_idx].resize(this->numChannels);
        this->gainModes[dir_idx].resize(this->numChannels);
        for (size_t chan_idx=0; chan_idx<this->numChannels; ++chan_idx) {
            this->gains[dir_idx][chan_idx].resize(this->numGains);
            for (size_t gain_idx=0; gain_idx<this->numGains; ++gain_idx) {
                this->gains[dir_idx][chan_idx][gain_idx] = 0.0f;
            }
            this->gainModes[dir_idx][chan_idx] = false;
        }
    }
}

SoapyLoopback::~SoapyLoopback(void) {
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyLoopback::getDriverKey(void) const {
    return "SoapyLoopbackDriver";
}

std::string SoapyLoopback::getHardwareKey(void) const {
    return "SoapyLoopback";
}

SoapySDR::Kwargs SoapyLoopback::getHardwareInfo(void) const {
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/juliatelecom/SoapyLoopback";
    args["identification"] = "loopback0";

    return args;
}


std::vector<SoapySDR::Kwargs> findLoopback(const SoapySDR::Kwargs & /*args*/) {
    std::vector<SoapySDR::Kwargs> results;

    SoapySDR::Kwargs devInfo;

    devInfo["label"] = "loopback_label";
    devInfo["product"] = "loopback_product";
    devInfo["serial"] = "loopback_serial";
    devInfo["manufacturer"] = "loopback_manufacturer";

    results.push_back(devInfo);
    return results;
}

/***********************************************************************
 * Make device instance
 **********************************************************************/

SoapySDR::Device *makeLoopback(const SoapySDR::Kwargs &args) {
    return new SoapyLoopback(args);
}


/***********************************************************************
 * Registration
 **********************************************************************/

static SoapySDR::Registry registerLoopback("Loopback", &findLoopback, &makeLoopback, SOAPY_SDR_ABI_VERSION);


/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyLoopback::getNumChannels(const int /*dir*/) const {
    return this->numChannels;
}

bool SoapyLoopback::getFullDuplex(const int /*direction*/, const size_t /*channel*/) const {
    return true;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listAntennas(const int direction, const size_t /*channel*/) const {
    std::vector<std::string> antennas;
    std::string prefix = "TX";
    if (direction == SOAPY_SDR_RX) {
        prefix = "RX";
    }
    for (size_t ant_idx=0; ant_idx < this->numAntennas; ++ant_idx) {
        antennas.push_back(prefix + std::to_string(ant_idx));
    }
    return antennas;
}

void SoapyLoopback::setAntenna(const int direction, const size_t channel, const std::string &name) {
    std::vector<std::string> antennas = this->listAntennas(direction, channel);
    for (auto &ant_name : antennas) {
        if (ant_name == name) {
            this->antennaSelections[direction][channel] = name;
            return;
        }
    }
    throw std::runtime_error("Invalid antenna name " + name + "!");
}

std::string SoapyLoopback::getAntenna(const int direction, const size_t channel) const {
    return this->antennaSelections[direction][channel];
}

/*******************************************************************
 * Gain API
 ******************************************************************/

bool SoapyLoopback::hasGainMode(const int /*direction*/, const size_t /*channel*/) const {
    return true;
}

void SoapyLoopback::setGainMode(const int direction, const size_t channel, const bool automatic) {
    this->gainModes[direction][channel] = automatic;
}

bool SoapyLoopback::getGainMode(const int direction, const size_t channel) const {
    return this->gainModes[direction][channel];
}

std::vector<std::string> SoapyLoopback::listGains(const int direction, const size_t /*channel*/) const {
    std::vector<std::string> gains;
    std::string prefix = "TX_GAIN";
    if (direction == SOAPY_SDR_RX) {
        prefix = "RX_GAIN";
    }
    for (size_t gain_idx=0; gain_idx<this->numGains; ++gain_idx) {
        gains.push_back(prefix + std::to_string(gain_idx));
    }
    return gains;
}

void SoapyLoopback::setGain(const int direction, const size_t channel, const std::string &name, const double value) {
    std::vector<std::string> gain_names = this->listGains(direction, channel);
    for (size_t gain_idx=0; gain_idx < this->numGains; ++gain_idx) {
        if (gain_names[gain_idx] == name) {
            this->gains[direction][channel][gain_idx] = value;
            return;
        }
    }
    throw std::runtime_error("Invalid gain name " + name + "!");
}

double SoapyLoopback::getGain(const int direction, const size_t channel, const std::string &name) const {
    std::vector<std::string> gain_names = this->listGains(direction, channel);
    for (size_t gain_idx=0; gain_idx < this->numGains; ++gain_idx) {
        if (gain_names[gain_idx] == name) {
            return this->gains[direction][channel][gain_idx];
        }
    }
    throw std::runtime_error("Invalid gain name " + name + "!");
}

SoapySDR::Range SoapyLoopback::getGainRange(const int direction, const size_t channel, const std::string &name) const {
    std::vector<std::string> gain_names = this->listGains(direction, channel);
    for (size_t gain_idx=0; gain_idx < this->numGains; ++gain_idx) {
        if (gain_names[gain_idx] == name) {
            return SoapySDR::Range(0, 10.0);
        }
    }
    throw std::runtime_error("Invalid gain name " + name + "!");
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listFrequencies(const int /*direction*/, const size_t /*channel*/) const {
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapyLoopback::getFrequencyRange(const int /*direction*/, const size_t /*channel*/, const std::string &name) const {
    SoapySDR::RangeList results;
    if (name == "RF") {
        // Simulate tuning from 30MHz to 3 GHz
        results.push_back(SoapySDR::Range(30e6, 3e9));
    }
    return results;
}

void SoapyLoopback::setFrequency(const int /*direction*/, const size_t /*channel*/,
                                 const std::string &name, const double frequency,
                                 const SoapySDR::Kwargs & /*args*/) {
    if (name == "RF") {
        this->frequency = frequency;
    } else {
        throw std::runtime_error("SoapyLoopback::setFrequency() Invalid frequency name " + name + "!");
    }
}

double SoapyLoopback::getFrequency(const int /*direction*/, const size_t /*channel*/, const std::string &name) const {
    if (name == "RF") {
        return this->frequency;
    } else {
        throw std::runtime_error("SoapyLoopback::getFrequency() Invalid frequency name " + name + "!");
    }
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

std::vector<double> SoapyLoopback::listSampleRates(const int /*direction*/, const size_t /*channel*/) const {
    std::vector<double> results;

    double master_rate = this->getMasterClockRate();
    double divider = 2;
    for (int pow_idx = 1; pow_idx < 9; pow_idx++) {
        divider *= 2;
        results.push_back(master_rate/divider);
    }
    return results;
}

void SoapyLoopback::setSampleRate(const int direction, const size_t channel, const double rate) {
    auto supported_samplerates = this->listSampleRates(direction, channel);
    for (auto supported_rate : supported_samplerates) {
        if (rate == supported_rate) {
            this->sampleRate = rate;
            return;
        }
    }
    throw std::runtime_error("SoapyLoopback::setSampleRate() unable to set samplerate of " + std::to_string(rate) + "!");
}

double SoapyLoopback::getSampleRate(const int /*direction*/, const size_t /*channel*/) const {
    return this->sampleRate;
}



SoapySDR::RangeList SoapyLoopback::getBandwidthRange(const int direction, const size_t channel) const {
    SoapySDR::RangeList results;
    results.push_back(SoapySDR::Range(0, this->getSampleRate(direction, channel)));
    return results;
}


void SoapyLoopback::setBandwidth(const int direction, const size_t channel, const double bw) {
    if (bw <= this->getSampleRate(direction, channel)) {
        this->bandwidth = bw;
    } else {
        throw std::runtime_error("SoapyLoopback::setBandwidth() unable to set bandwidth of " + std::to_string(bw) + "!");
    }
}

double SoapyLoopback::getBandwidth(const int /*direction*/, const size_t /*channel*/) const {
    return bandwidth;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyLoopback::getSettingInfo(void) const {
    SoapySDR::ArgInfoList setArgs;

    SoapySDR::ArgInfo iqSwapArg;
    iqSwapArg.key = "iq_swap";
    iqSwapArg.value = "false";
    iqSwapArg.name = "I/Q Swap";
    iqSwapArg.description = "I/Q Swap Mode";
    iqSwapArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(iqSwapArg);
    return setArgs;
}

void SoapyLoopback::writeSetting(const std::string &key, const std::string &value) {
    if (key == "iq_swap") {
        iqSwap = ((value=="true") ? true : false);
        SoapySDR_logf(SOAPY_SDR_DEBUG, "[SoapyLoopback] I/Q swap: %s", iqSwap ? "true" : "false");
    } else {
        SoapySDR_logf(SOAPY_SDR_WARNING, "[SoapyLoopback] writeSetting(): Unknown setting: '%s' => '%s'", key.c_str(), value.c_str());
    }
}

std::string SoapyLoopback::readSetting(const std::string &key) const {
    if (key == "iq_swap") {
        return this->iqSwap ? "true" : "false";
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}

/*******************************************************************
 * Clocking API
 ******************************************************************/

SoapySDR::RangeList SoapyLoopback::getMasterClockRates(void) const {
	SoapySDR::RangeList rates;
    // 10MHz to 52MHz
	rates.push_back(SoapySDR::Range(10e6, 52e6));
	return rates;
}

void SoapyLoopback::setMasterClockRate(const double rate) {
    SoapySDR::RangeList rate_list = this->getMasterClockRates();
    for (auto rate_range : rate_list) {
        if (rate >= rate_range.minimum() && rate <= rate_range.maximum()) {
            this->masterClockRate = rate;
            return;
        }
    }
    throw std::runtime_error("SoapyLoopback::setMasterClockRate() unable to set clock rate of " + std::to_string(rate) + "!");
}

double SoapyLoopback::getMasterClockRate(void) const {
	return this->masterClockRate;
}

std::vector<std::string> SoapyLoopback::listClockSources(void) const {
	return { "internal", "external" };
}

void SoapyLoopback::setClockSource(const std::string &source) {
    std::vector<std::string> source_names = this->listClockSources();
    for (auto name : source_names) {
        if (name == source) {
            this->clockSource = source;
            return;
        }
    }
    throw std::runtime_error("SoapyLoopback::setClockSource() unable to set clock source '" + source + "'!");
}

std::string SoapyLoopback::getClockSource(void) const {
    return this->clockSource;
}


/*******************************************************************
 * Sensor API
 ******************************************************************/

std::vector<std::string> SoapyLoopback::listSensors(void) const {
	std::vector<std::string> sensors;
	sensors.push_back("clock_locked");
	return sensors;
}

SoapySDR::ArgInfo SoapyLoopback::getSensorInfo(const std::string &name) const {
	SoapySDR::ArgInfo info;
	if (name == "clock_locked") {
		info.key = "clock_locked";
		info.name = "Clock Locked";
		info.type = SoapySDR::ArgInfo::BOOL;
		info.value = "false";
		info.description = "CGEN clock is locked, good VCO selection.";
	}
	return info;
}

std::string SoapyLoopback::readSensor(const std::string &name) const {
	if (name == "clock_locked") {
		return "true";
	}
	throw std::runtime_error("SoapyLoopback::readSensor() - unknown sensor '" + name + "'!");
}

std::vector<std::string> SoapyLoopback::listSensors(const int /*direction*/, const size_t /*channel*/) const {
	std::vector<std::string> sensors;
	sensors.push_back("lo_locked");
	return sensors;
}

SoapySDR::ArgInfo SoapyLoopback::getSensorInfo(const int /*direction*/, const size_t /*channel*/, const std::string &name) const {
	SoapySDR::ArgInfo info;
	if (name == "lo_locked") {
		info.key = "lo_locked";
		info.name = "Local Oscillator Locked";
		info.type = SoapySDR::ArgInfo::BOOL;
		info.value = "false";
		info.description = "LO synthesizer is locked, good VCO selection.";
	}
	return info;
}

std::string SoapyLoopback::readSensor(const int /*direction*/, const size_t /*channel*/, const std::string &name) const {
	if (name == "lo_locked") {
		return "true";
	}

	throw std::runtime_error("SoapyLoopback::readSensor() - unknown sensor '"+ name + "'!");
}
