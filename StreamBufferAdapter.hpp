#include <vector>
#include <stdint.h>
#include <functional>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Errors.hpp>

//typedef int (*AcquireBufferFunc)(std::vector<uint8_t *> &buffs);
//typedef int (*ReleaseBufferFunc)(std::vector<uint8_t *> &buffs);
typedef std::function<int(std::vector<uint8_t *> &buffs)> AcquireBufferFunc;
typedef std::function<int(std::vector<uint8_t *> &buffs)> ReleaseBufferFunc;

/*
 * This class acts as an adapter from a strictly buffered API
 * (e.g. a DMA engine) to a streaming API (e.g. SoapySDR).
 * Feed buffers in from the DMA engine when necessary, and
 * read samples out piecemeal for reading, or feed samples
 * in piecemeal and only transmit buffers when they're full
 * for sending.
 */
class StreamBufferAdapter {
    public:
        StreamBufferAdapter(AcquireBufferFunc acquire_buff_func, ReleaseBufferFunc release_buff_func, size_t buff_len, size_t elem_size) :
                            acquire_buff_func(acquire_buff_func), release_buff_func(release_buff_func),
                            buff_len(buff_len), buff_usage(0), elem_size(elem_size) {
        }
        ~StreamBufferAdapter() {
            if (!this->buffs.empty()) {
                this->release_buff_func(this->buffs);
            }
        }

        size_t getBuffLen() {
            return this->buff_len;
        }
        size_t getElemSize() {
            return this->elem_size;
        }
        size_t getBuffUsage() {
            return this->buff_usage;
        }
        size_t getBuffSpace() {
            return getBuffLen() - getBuffUsage();
        }

        /*
         * `transact()` is used for both reading and writing; if `read = true`, then we
         * copy from the internal buffer into `data`, if `read = false`, then we copy
         * from `data` into the internal buffer.  In all cases, we acquire new buffers
         * from the provided function pointers when needed, and the number of samples
         * actually transacted is returned in `num_samples`.  Note that you may need to
         * call `transact()` multiple times to actually commit all samples, as
         * `transact()` may commit fewer samples than requested.
         */
        int transact(std::vector<uint8_t *> &data, size_t *num_samples, bool read) {
            if (this->buffs.empty()) {
                // If there's no buffer to transact with, attempt to acquire one
                int ret = this->acquire_buff_func(this->buffs);
                this->buff_usage = 0;

                // If we couldn't get a buffer, toss the error up the chain
                if (ret != 0) {
                    *num_samples = 0;
                    return ret;
                }
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Successfully acquired with %lu channels", this->buffs.size());
            }

            // If someone lied about how many channels to deal with, complain
            if (data.size() != this->buffs.size()) {
                throw std::runtime_error("Attempted to transact invalid number of channels ("
                    + std::to_string(data.size()) + " != " + std::to_string(this->buffs.size()) + ")");
                return SOAPY_SDR_STREAM_ERROR;
            }

            // We write only as much as we can, given our current usage
            *num_samples = std::min(this->buff_len - this->buff_usage, *num_samples);
            size_t offset = this->buff_usage * this->elem_size;

            // For debugging's sake, print out the first/last sample of the first channel, offset, size, etc...
            if (read) {
                SoapySDR_logf(SOAPY_SDR_DEBUG,
                    "SBA: read() -> [%f + %fim, ..., %f + %fim] (length: %zu, channels: %zu, offset: %zu)\n",
                    ((float *)this->buffs[0])[offset/sizeof(float) + 0],
                    ((float *)this->buffs[0])[offset/sizeof(float) + 1],
                    ((float *)this->buffs[0])[offset/sizeof(float) + (*num_samples-1)*2+0],
                    ((float *)this->buffs[0])[offset/sizeof(float) + (*num_samples-1)*2+1],
                    *num_samples,
                    data.size(),
                    offset
                );
            } else {
                SoapySDR_logf(SOAPY_SDR_DEBUG,
                    "SBA: write() -> [%f + %fim, ..., %f + %fim] (length: %zu, channels: %zu, offset: %zu)\n",
                    ((float *)data[0])[0],
                    ((float *)data[0])[1],
                    ((float *)data[0])[(*num_samples-1)*2+0],
                    ((float *)data[0])[(*num_samples-1)*2+1],
                    *num_samples,
                    data.size(),
                    offset
                );
            }

            for (size_t chan_idx=0; chan_idx < data.size(); ++chan_idx) {
                if (read) {
                    memcpy(data[chan_idx], this->buffs[chan_idx] + offset, (*num_samples)*this->elem_size);
                } else {
                    memcpy(this->buffs[chan_idx] + offset, data[chan_idx],        (*num_samples)*this->elem_size);
                }
            }
            this->buff_usage += *num_samples;

            // If our current buffer is full, release it, so that it can be used.
            // Future invocations of `transact()` will acquire a new buffer.
            if (this->buff_len - this->buff_usage == 0) {
                int ret = this->release_buff_func(this->buffs);
                if (ret != 0) {
                    return ret;
                }
            }
            return 0;
        }
    private:
        AcquireBufferFunc acquire_buff_func;
        ReleaseBufferFunc release_buff_func;
        size_t buff_len, buff_usage, elem_size;
        std::vector<uint8_t *> buffs;
};
