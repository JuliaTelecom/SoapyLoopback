/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2015-2017 Josh Blum
 * Copyright (c) 2022 Julia Computing

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
#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Types.h>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// How many channels does this Loopback contain?
#define DEFAULT_NUM_CHANNELS                2

// How many samples in a single buffer
#define DEFAULT_BUFFER_LENGTH_IN_SAMPLES    1024

// How many bytes? (requires a type value T)
#define DEFAULT_BUFFER_LENGTH_IN_BYTES(T)   (sizeof(T) * DEFAULT_NUM_CHANNELS * DEFAULT_BUFFER_LENGTH_IN_SAMPLES)

// How many buffers in the ring buffer
#define DEFAULT_NUM_BUFFERS                 32

// How many antenna choices for Tx/Rx?
#define DEFAULT_NUM_ANTENNAS                3

// How many gain choices for Tx/Rx?
#define DEFAULT_NUM_GAINS                   3

#include "LoopbackRingBuffer.hpp"
#include "StreamBufferAdapter.hpp"

class SoapyLoopback : public SoapySDR::Device
{
public:
    SoapyLoopback(const SoapySDR::Kwargs &args);
    ~SoapyLoopback(void);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const override;
    std::string getHardwareKey(void) const override;
    SoapySDR::Kwargs getHardwareInfo(void) const override;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int) const override;
    bool getFullDuplex(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const override;
    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const override;
    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const override;
    SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                  const std::vector<size_t> &channels = std::vector<size_t>(),
                                  const SoapySDR::Kwargs &args = SoapySDR::Kwargs()) override;

    void closeStream(SoapySDR::Stream *stream) override;

    size_t getStreamMTU(SoapySDR::Stream *stream) const override;

    int activateStream(
            SoapySDR::Stream *stream,
            const int flags = 0,
            const long long timeNs = 0,
            const size_t numElems = 0) override;

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0) override;

    int readStream(
            SoapySDR::Stream *stream,
            void * const *buffs,
            const size_t numElems,
            int &flags,
            long long &timeNs,
            const long timeoutUs = 100000) override;
    int writeStream(
            SoapySDR::Stream *stream,
            const void * const *buffs,
            const size_t numElems,
            int &flags,
            const long long timeNs = 0,
            const long timeoutUs = 100000) override;

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const override;
    void setAntenna(const int direction, const size_t channel, const std::string &name) override;
    std::string getAntenna(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    //bool hasDCOffsetMode(const int direction, const size_t channel) const override;
    //bool hasFrequencyCorrection(const int direction, const size_t channel) const override;
    //void setFrequencyCorrection(const int direction, const size_t channel, const double value) override;
    //double getFrequencyCorrection(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;
    bool hasGainMode(const int direction, const size_t channel) const override;
    void setGainMode(const int direction, const size_t channel, const bool automatic) override;
    bool getGainMode(const int direction, const size_t channel) const override;
    void setGain(const int direction, const size_t channel, const std::string &name, const double value) override;
    double getGain(const int direction, const size_t channel, const std::string &name) const override;
    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction, const size_t channel, const std::string &name,
                      const double frequency, const SoapySDR::Kwargs &args = SoapySDR::Kwargs()) override;
    double getFrequency(const int direction, const size_t channel, const std::string &name) const override;
    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const override;
    //SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate) override;
    double getSampleRate(const int direction, const size_t channel) const override;
    std::vector<double> listSampleRates(const int direction, const size_t channel) const override;
    //SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const override;
    void setBandwidth(const int direction, const size_t channel, const double bw) override;
    double getBandwidth(const int direction, const size_t channel) const override;
    //std::vector<double> listBandwidths(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Time API
     ******************************************************************/

    //std::vector<std::string> listTimeSources(void) const override;
    //std::string getTimeSource(void) const override;
    //void setTimeSource(const std::string &what = "") override;
    //bool hasHardwareTime(const std::string &what = "") const override;
    //long long getHardwareTime(const std::string &what = "") const override;
    //void setHardwareTime(const long long timeNs, const std::string &what = "") override;

    /*******************************************************************
     * Clocking API
     ******************************************************************/

    void setMasterClockRate(const double rate) override;
    double getMasterClockRate(void) const override;
    SoapySDR::RangeList getMasterClockRates(void) const override;
    std::vector<std::string> listClockSources(void) const override;
    void setClockSource(const std::string &source) override;
    std::string getClockSource(void) const override;

    /*******************************************************************
     * Sensor API
     ******************************************************************/

    std::vector<std::string> listSensors(void) const override;
    SoapySDR::ArgInfo getSensorInfo(const std::string &name) const override;
    std::string readSensor(const std::string &name) const override;
    std::vector<std::string> listSensors(const int direction, const size_t channel) const override;
    SoapySDR::ArgInfo getSensorInfo(const int direction, const size_t channel, const std::string &name) const override;
    std::string readSensor(const int direction, const size_t channel, const std::string &name) const override;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const override;
    void writeSetting(const std::string &key, const std::string &value) override;
    std::string readSetting(const std::string &key) const override;

private:
    int transact(void ** output_buffs, const size_t numElems, int &flags, const long timeoutUs, bool read);
    // Device/data sizing.  Note that in generally we're always symmetric;
    // We don't bother to simulate e.g. 1 TX and 2 RX devices.
    size_t numChannels, numAntennas, numGains;
    std::string sampleFormat;

    // Configuration stores, typically per-direction and per-channel.
    std::vector<std::vector<std::string>> antennaSelections;
    std::vector<std::vector<std::vector<double>>> gains;
    std::vector<std::vector<bool>> gainModes;

    // Tuner settings.  We lock the tuner across directions and channels.
    std::string clockSource;
    double frequency, sampleRate, bandwidth, masterClockRate;
    bool iqSwap;

    // For read/write APIs.  Only instantiated upon full duplex stream activation.
    LoopbackRingBuffer *ringBuff;
    StreamBufferAdapter *readAdapter, *writeAdapter;
};
