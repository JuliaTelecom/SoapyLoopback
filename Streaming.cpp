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


std::vector<std::string> SoapyLoopback::getStreamFormats(const int /*direction*/, const size_t /*channel*/) const {
    std::vector<std::string> formats;

    //formats.push_back(SOAPY_SDR_CS8);
    //formats.push_back(SOAPY_SDR_CS12);
    //formats.push_back(SOAPY_SDR_CS16);
    formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string SoapyLoopback::getNativeStreamFormat(const int /*direction*/, const size_t /*channel*/, double &fullScale) const {
    fullScale = 1.0;
    return SOAPY_SDR_CF32;
}

SoapySDR::ArgInfoList SoapyLoopback::getStreamArgsInfo(const int /*direction*/, const size_t /*channel*/) const {
    SoapySDR::ArgInfoList streamArgs;

    SoapySDR::ArgInfo bufflenArg;
    bufflenArg.key = "bufflen";
    bufflenArg.value = std::to_string(DEFAULT_BUFFER_LENGTH_IN_BYTES(float));
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

    return streamArgs;
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
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0)) {
        throw std::runtime_error("setupStream: invalid channel selection");
    }

    // TODO: actually support more formats, store into rxFormat, etc....
    if (format == SOAPY_SDR_CF32) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        //rxFormat = RTL_RX_FORMAT_FLOAT32;
    }
    /*
    } else if (format == SOAPY_SDR_CS12) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS12.");
        //rxFormat = RTL_RX_FORMAT_INT16;
    } else if (format == SOAPY_SDR_CS16) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        //rxFormat = RTL_RX_FORMAT_INT16;
    } else if (format == SOAPY_SDR_CS8) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS8.");
        //rxFormat = RTL_RX_FORMAT_INT8;
    */
    else {
        throw std::runtime_error(
                "setupStream: invalid format '" + format
                        + "' -- Only CS8, CS16 and CF32 are supported by SoapyLoopback module.");
    }

    size_t bufflen = DEFAULT_BUFFER_LENGTH_IN_SAMPLES;
    if (args.count("bufflen") != 0) {
        try {
            size_t bufflen_in = std::stoi(args.at("bufflen"));
            if (bufflen_in > 0) {
                bufflen = bufflen_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "[SoapyLoopback] Using buffer length %d", bufflen);

    size_t num_buffers = DEFAULT_NUM_BUFFERS;
    if (args.count("buffers") != 0) {
        try {
            int num_buffers_in = std::stoi(args.at("buffers"));
            if (num_buffers_in > 0) {
                num_buffers = num_buffers_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "SoapyLoopback Using %zu buffers", num_buffers);

    // If we already have a ringBuff allocated, ensure we match it, otherwise create one:
    if (this->ringBuff == NULL) {
        // Allocate ring buffer with these values
        this->ringBuff = new LoopbackRingBuffer(num_buffers, bufflen, channels.size(), SoapySDR::formatToSize(this->sampleFormat));
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RingBuffer constructed");
    } else {
        if (this->ringBuff->numBuffers() != num_buffers) {
            throw std::runtime_error(
                "Cannot open stream that disagrees in buffer count (" +
                std::to_string(num_buffers) +
                " != " +
                std::to_string(this->ringBuff->numBuffers()) +
                ") with previously-setup stream"
            );
        }
        if (this->ringBuff->bufferLen() != bufflen) {
            throw std::runtime_error(
                "Cannot open stream that disagrees in buffer length (" +
                std::to_string(bufflen) +
                " != " +
                std::to_string(this->ringBuff->bufferLen()) +
                ") with previously-setup stream"
            );
        }
        if (this->ringBuff->numChannels() != channels.size()) {
            throw std::runtime_error(
                "Cannot open stream that disagrees in buffer length (" +
                std::to_string(channels.size()) +
                " != " +
                std::to_string(this->ringBuff->numChannels()) +
                ") with previously-setup stream"
            );
        }
        auto elem_size = SoapySDR::formatToSize(this->sampleFormat);
        if (this->ringBuff->elemSize() != elem_size) {
            throw std::runtime_error(
                "Cannot open stream that disagrees in element size (" +
                std::to_string(elem_size) +
                " != " +
                std::to_string(this->ringBuff->elemSize()) +
                ") with previously-setup stream"
            );
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "RingBuffer congruency validated");
    }

    // Differentiate rx/tx direction by returning two different stream "pointer values"
    // We just return `(this) + direction`, so that we have differentiated, non-NULL values.
    // These pointers should never be dereferenced, so we're okay to use whatever we want here.
    return (SoapySDR::Stream *) ((uintptr_t)this + (uintptr_t)direction);
}

void SoapyLoopback::closeStream(SoapySDR::Stream *stream) {
    this->deactivateStream(stream, 0, 0);

    // Only free the ring buffer if we've closed all opened streams
    if (this->readAdapter == NULL && this->writeAdapter == NULL) {
        delete this->ringBuff;
        this->ringBuff = NULL;
    }
}

size_t SoapyLoopback::getStreamMTU(SoapySDR::Stream * /*stream*/) const {
    return this->ringBuff->bufferLen();
}

int SoapyLoopback::activateStream(
        SoapySDR::Stream * stream,
        const int /*flags*/,
        const long long /*timeNs*/,
        const size_t numElems) {
    
    // Ensure that `numElems` matches what was provided above as `bufflen`
    if (numElems != this->ringBuff->bufferLen()) {
        throw std::runtime_error(
            "activateStream mismatched numElems (" + std::to_string(numElems) +
            ") with ring buffer bufflen (" + std::to_string(this->ringBuff->bufferLen()) + ")"
        );
    }

    // Set up stream buffer adapters, to allow for easy reading and writing of samples.
    // We track initialization of rx/tx streams by activating either the read or write adapters:
    if ((uintptr_t)stream == (uintptr_t)this + (uintptr_t)SOAPY_SDR_RX) {
        auto read_acq = [this](std::vector<uint8_t *> &buffs) { return this->ringBuff->acquireReadBuffer(buffs); };
        auto read_rel = [this](std::vector<uint8_t *> &buffs) { return this->ringBuff->releaseReadBuffer(buffs); };
        this->readAdapter = new StreamBufferAdapter(read_acq, read_rel, numElems, SoapySDR::formatToSize(this->sampleFormat));
    } else if ((uintptr_t)stream == (uintptr_t)this + (uintptr_t)SOAPY_SDR_TX) {
        auto write_acq = [this](std::vector<uint8_t *> &buffs) { return this->ringBuff->acquireWriteBuffer(buffs); };
        auto write_rel = [this](std::vector<uint8_t *> &buffs) { return this->ringBuff->releaseWriteBuffer(buffs); };
        this->writeAdapter = new StreamBufferAdapter((AcquireBufferFunc)write_acq, (ReleaseBufferFunc)write_rel, numElems, SoapySDR::formatToSize(this->sampleFormat));
    } else {
        throw std::runtime_error(
            "activateStream called with invalid stream (" + std::to_string((uintptr_t)stream) + ")"
        );
    }
    return 0;
}

int SoapyLoopback::deactivateStream(SoapySDR::Stream *stream, const int /*flags*/, const long long /*timeNs*/) {
    if ((uintptr_t)stream == (uintptr_t)this + (uintptr_t)SOAPY_SDR_RX) {
        if (this->readAdapter != NULL) {
            delete this->readAdapter;
            this->readAdapter = NULL;
        }
    } else if ((uintptr_t)stream == (uintptr_t)this + (uintptr_t)SOAPY_SDR_TX) {
        if (this->writeAdapter != NULL) {
            delete this->writeAdapter;
            this->writeAdapter = NULL;
        }
    } else {
        throw std::runtime_error(
            "deactivateStream called with invalid stream (" + std::to_string((uintptr_t)stream) + ")"
        );
    }

    return 0;
}

int SoapyLoopback::transact(void ** output_buffs, const size_t numElems, int &flags,
                            const long timeoutUs, bool read) {
    // Convert from bare array to `std::vector<>`
    std::vector<uint8_t *> buffs(this->ringBuff->numChannels());
    for (size_t chan_idx=0; chan_idx<this->ringBuff->numChannels(); ++chan_idx) {
        buffs[chan_idx] = (uint8_t *)output_buffs[chan_idx];
    }

    StreamBufferAdapter * adapter = read ? this->readAdapter : this->writeAdapter;
    size_t num_samples = numElems;

    auto t_start = std::chrono::high_resolution_clock::now();
    while (true) {
        // If we successfully transact, return how many samples we were able to transmit
        if (adapter->transact(buffs, &num_samples, read) == 0) {
            // When reading, report that we have more fragments if we haven't completely
            // consumed the buffer.
            if (read && adapter->getBuffSpace() > 0) {
                flags |= SOAPY_SDR_MORE_FRAGMENTS;
            }
            return num_samples;
        }

        // If we time out, return `SOAPY_SDR_TIMEOUT`.
        auto t_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t_start);
        if (t_duration.count() >= timeoutUs) {
            return SOAPY_SDR_TIMEOUT;
        }
    }
    // We should never get here.
    return SOAPY_SDR_STREAM_ERROR;
}

int SoapyLoopback::readStream(
        SoapySDR::Stream * /*stream*/,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long & /*timeNs*/,
        const long timeoutUs)
{
    // Drop the `const` qualifiers on `buffs`, as we don't want to have
    // to duplicate a bunch of code; because of our `read` boolean parameter
    // we know when we'll write into `buffs` and when we won't, but the
    // compiler can't quite tell, so just typecast to assuage its doubts.
    return this->transact( (void**)buffs, numElems, flags, timeoutUs, true);
}

int SoapyLoopback::writeStream(
        SoapySDR::Stream * /*stream*/,
        const void * const *buffs,
        const size_t numElems,
        int &flags,
        const long long /*timeNs*/,
        const long timeoutUs)
{
    // Same here as above.
    return this->transact((void**)buffs, numElems, flags, timeoutUs, false);
}
