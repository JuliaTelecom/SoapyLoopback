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
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Time.hpp>
#include <algorithm> //min
#include <climits> //SHRT_MAX
#include <cstring> // memcpy


std::vector<std::string> SoapyLoopback::getStreamFormats(const int direction, const size_t channel) const {
    std::vector<std::string> formats;

    formats.push_back(SOAPY_SDR_CS8);
    formats.push_back(SOAPY_SDR_CS12);
    formats.push_back(SOAPY_SDR_CS16);
    formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string SoapyLoopback::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const {

     return SOAPY_SDR_CS12;
}

SoapySDR::ArgInfoList SoapyLoopback::getStreamArgsInfo(const int direction, const size_t channel) const {
    SoapySDR::ArgInfoList streamArgs;

    SoapySDR::ArgInfo bufflenArg;
    bufflenArg.key = "bufflen";
    bufflenArg.value = std::to_string(DEFAULT_BUFFER_LENGTH);
    bufflenArg.name = "Buffer Size";
    bufflenArg.description = "Number of bytes per buffer, multiples of 512 only.";
    bufflenArg.units = "bytes";
    bufflenArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(bufflenArg);

    SoapySDR::ArgInfo buffersArg;
    buffersArg.key = "buffers";
    buffersArg.value = std::to_string(DEFAULT_NUM_BUFFERS);
    buffersArg.name = "Ring buffers";
    buffersArg.description = "Number of buffers in the ring.";
    buffersArg.units = "buffers";
    buffersArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(buffersArg);

    SoapySDR::ArgInfo asyncbuffsArg;
    asyncbuffsArg.key = "asyncBuffs";
    asyncbuffsArg.value = "0";
    asyncbuffsArg.name = "Async buffers";
    asyncbuffsArg.description = "Number of async usb buffers (advanced).";
    asyncbuffsArg.units = "buffers";
    asyncbuffsArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(asyncbuffsArg);

    return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

void SoapyLoopback::rx_async_operation(void)
{
    //printf("rx_async_operation\n");
    //rtlsdr_read_async(dev, &_rx_callback, this, asyncBuffs, bufferLength);
    //printf("rx_async_operation done!\n");
}

void SoapyLoopback::rx_callback(unsigned char *buf, uint32_t len)
{
    //printf("_rx_callback %d _buf_head=%d, numBuffers=%d\n", len, _buf_head, _buf_tail);

    // atomically add len to ticks but return the previous value
    unsigned long long tick = ticks.fetch_add(len);

    //overflow condition: the caller is not reading fast enough
    if (_buf_count == numBuffers)
    {
        _overflowEvent = true;
        return;
    }

    //copy into the buffer queue
    auto &buff = _buffs[_buf_tail];
    buff.tick = tick;
    buff.data.resize(len);
    std::memcpy(buff.data.data(), buf, len);

    //increment the tail pointer
    _buf_tail = (_buf_tail + 1) % numBuffers;

    //increment buffers available under lock
    //to avoid race in acquireReadBuffer wait
    {
    std::lock_guard<std::mutex> lock(_buf_mutex);
    _buf_count++;

    }

    //notify readStream()
    _buf_cond.notify_one();
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDR::Stream *SoapyLoopback::setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args)
{

    //check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }

    //check the format
    if (format == SOAPY_SDR_CF32)
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        //rxFormat = RTL_RX_FORMAT_FLOAT32;
    }
    else if (format == SOAPY_SDR_CS12)
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS12.");
        //rxFormat = RTL_RX_FORMAT_INT16;
    }
    else if (format == SOAPY_SDR_CS16)
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        //rxFormat = RTL_RX_FORMAT_INT16;
    }
    else if (format == SOAPY_SDR_CS8) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS8.");
        //rxFormat = RTL_RX_FORMAT_INT8;
    }
    else
    {
        throw std::runtime_error(
                "setupStream invalid format '" + format
                        + "' -- Only CS8, CS16 and CF32 are supported by SoapyLoopback module.");
    }

    bufferLength = DEFAULT_BUFFER_LENGTH;
    if (args.count("bufflen") != 0)
    {
        try
        {
            int bufferLength_in = std::stoi(args.at("bufflen"));
            if (bufferLength_in > 0)
            {
                bufferLength = bufferLength_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using buffer length %d", bufferLength);

    numBuffers = DEFAULT_NUM_BUFFERS;
    if (args.count("buffers") != 0)
    {
        try
        {
            int numBuffers_in = std::stoi(args.at("buffers"));
            if (numBuffers_in > 0)
            {
                numBuffers = numBuffers_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using %d buffers", numBuffers);

    asyncBuffs = 0;
    if (args.count("asyncBuffs") != 0)
    {
        try
        {
            int asyncBuffs_in = std::stoi(args.at("asyncBuffs"));
            if (asyncBuffs_in > 0)
            {
                asyncBuffs = asyncBuffs_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    //if (tunerType == RTLSDR_TUNER_E4000) {
    //    IFGain[0] = 6;
    //    IFGain[1] = 9;
    //    IFGain[2] = 3;
    //    IFGain[3] = 2;
    //    IFGain[4] = 3;
    //    IFGain[5] = 3;
    //} else {
    //    for (int i = 0; i < 6; i++) {
    //        IFGain[i] = 0;
    //    }
    //}
    //tunerGain = rtlsdr_get_tuner_gain(dev) / 10.0;

    //clear async fifo counts
    _buf_tail = 0;
    _buf_count = 0;
    _buf_head = 0;

    //allocate buffers
    _buffs.resize(numBuffers);
    for (auto &buff : _buffs) buff.data.reserve(bufferLength);
    for (auto &buff : _buffs) buff.data.resize(bufferLength);

    return (SoapySDR::Stream *) this;
}

void SoapyLoopback::closeStream(SoapySDR::Stream *stream)
{
    this->deactivateStream(stream, 0, 0);
    _buffs.clear();
}

size_t SoapyLoopback::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferLength / BYTES_PER_SAMPLE;
}

int SoapyLoopback::activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems)
{
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;
    resetBuffer = true;
    bufferedElems = 0;

    //start the async thread
    if (not _rx_async_thread.joinable())
    {
        _rx_async_thread = std::thread(&SoapyLoopback::rx_async_operation, this);
    }

    return 0;
}

int SoapyLoopback::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;
    if (_rx_async_thread.joinable())
    {
        _rx_async_thread.join();
    }
    return 0;
}

int SoapyLoopback::readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs)
{
    //drop remainder buffer on reset
    if (resetBuffer and bufferedElems != 0)
    {
        bufferedElems = 0;
        this->releaseReadBuffer(stream, _currentHandle);
    }

    //this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    //are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, _currentHandle, (const void **)&_currentBuff, flags, timeNs, timeoutUs);
        if (ret < 0) return ret;
        bufferedElems = ret;
    }

    //otherwise just update return time to the current tick count
    else
    {
        flags |= SOAPY_SDR_HAS_TIME;
        timeNs = SoapySDR::ticksToTimeNs(bufTicks, sampleRate);
    }

    size_t returnedElems = std::min(bufferedElems, numElems);

    //bump variables for next call into readStream
    bufferedElems -= returnedElems;
    _currentBuff += returnedElems*BYTES_PER_SAMPLE;
    bufTicks += returnedElems; //for the next call to readStream if there is a remainder

    //return number of elements written to buff0
    if (bufferedElems != 0) flags |= SOAPY_SDR_MORE_FRAGMENTS;
    else this->releaseReadBuffer(stream, _currentHandle);
    return returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyLoopback::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    return _buffs.size();
}

int SoapyLoopback::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    buffs[0] = (void *)_buffs[handle].data.data();
    return 0;
}

int SoapyLoopback::acquireReadBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    const void **buffs,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    //reset is issued by various settings
    //to drain old data out of the queue
    if (resetBuffer)
    {
        //drain all buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        resetBuffer = false;
        _overflowEvent = false;
    }

    //handle overflow from the rx callback thread
    if (_overflowEvent)
    {
        //drain the old buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        _overflowEvent = false;
        SoapySDR::log(SOAPY_SDR_SSI, "O");
        return SOAPY_SDR_OVERFLOW;
    }

    //wait for a buffer to become available
    if (_buf_count == 0)
    {
        std::unique_lock <std::mutex> lock(_buf_mutex);
        _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs), [this]{return _buf_count != 0;});
        if (_buf_count == 0) return SOAPY_SDR_TIMEOUT;
    }

    //extract handle and buffer
    handle = _buf_head;
    _buf_head = (_buf_head + 1) % numBuffers;
    bufTicks = _buffs[handle].tick;
    timeNs = SoapySDR::ticksToTimeNs(_buffs[handle].tick, sampleRate);
    buffs[0] = (void *)_buffs[handle].data.data();
    flags = SOAPY_SDR_HAS_TIME;

    //return number available
    return _buffs[handle].data.size() / BYTES_PER_SAMPLE;
}

void SoapyLoopback::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    //TODO this wont handle out of order releases
    _buf_count--;
}
