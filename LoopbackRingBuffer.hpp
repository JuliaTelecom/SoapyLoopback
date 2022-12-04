#include <vector>
#include <stdint.h>
#include <SoapySDR/Errors.hpp>

/*
 * LoopbackRingBuffer
 *
 * This represents a ring buffer that loops writes back to future reads; it contains
 * a fixed internal capacity (`num_buffers`), and only allows writes/reads in full
 * buffer chunks, controlled by `num_samples`, `num_channels` and `elem_size`.
 * It provides a `uint8_t *`-based buffer interface, intended for `memcpy()` and friends,
 * instead of something more exotic for different sample types.
 * 
 * It uses an `acquireBuffer()`/`releaseBuffer()` API that fills out a provided
 * `std::vector<uint8_t *>` with the buffers to read out of/write into.  It is
 * suggested to use `StreamBufferAdapter` alongside this class to provide a more familiar
 * `read()`/`write()` API with arbitrary-length reads and writes.
 */
class LoopbackRingBuffer {
    public:
        LoopbackRingBuffer(const size_t num_buffers,  const size_t num_samples,
                           const size_t num_channels, const size_t elem_size) :
                           buffer_len(num_samples), elem_size(elem_size),
                           write_idx(0), read_idx(0) {
            // Initialize memory for our buffers
            this->buffers.resize(num_buffers);
            for (auto &channel_buffer : this->buffers) {
                channel_buffer.resize(num_channels);
                for (size_t chan_idx=0; chan_idx<num_channels; ++chan_idx) {
                    channel_buffer[chan_idx] = new uint8_t[buffer_len*elem_size];
                }
            }
        }
        ~LoopbackRingBuffer() {
            this->buffers.clear();
        }

        size_t buffersAvailableToRead() {
            return (this->write_idx - this->read_idx)%this->buffers.size();
        }
        size_t buffersAvailableToWrite() {
            return this->buffers.size() - this->buffersAvailableToRead() - 1;
        }

        size_t numBuffers() {
            return this->buffers.size();
        }
        size_t numChannels() {
            return this->buffers[0].size();
        }
        size_t bufferLen() {
            return this->buffer_len;
        }
        size_t elemSize() {
            return this->elem_size;
        }
        size_t bufferSize() {
            return bufferLen() * elemSize();
        }

        int acquireBuffer(std::vector<uint8_t *> &buffs, size_t buffs_available, size_t buff_idx) {
            buffs.clear();
            // If we have no buffers available to read/write, return an empty list, and surface an error code.
            if (buffs_available == 0) {
                return SOAPY_SDR_UNDERFLOW;
            }
            for (auto buff : this->buffers[buff_idx]) {
                buffs.push_back(buff);
            }
            // zero means success in SoapySDR-land
            return 0;
        }

        int releaseBuffer(std::vector<uint8_t *> &buffs, size_t *buff_idx) {
            const char * bad_release_msg = "releaseBuffer() called in an illegal manner (perhaps multiple acquisitions in-flight?)";
            // We only allow a single acquireReadBuffer in flight at a time
            if (buffs.size() != this->buffers[*buff_idx].size()) {
                throw std::runtime_error(bad_release_msg);
            }
            for (size_t chan_idx=0; chan_idx<this->buffers[*buff_idx].size(); ++chan_idx) {
                if (buffs[chan_idx] != this->buffers[*buff_idx][chan_idx]) {
                    throw std::runtime_error(bad_release_msg);
                }
            }
            buffs.clear();
            *buff_idx += 1;
            return 0;
        }

        int acquireReadBuffer(std::vector<uint8_t *> &buffs) {
            return this->acquireBuffer(buffs, this->buffersAvailableToRead(), this->read_idx);
        }

        int releaseReadBuffer(std::vector<uint8_t *> &buffs) {
            return this->releaseBuffer(buffs, &this->read_idx);
        }

        int acquireWriteBuffer(std::vector<uint8_t *> &buffs) {
            return this->acquireBuffer(buffs, this->buffersAvailableToWrite(), this->write_idx);
        }

        int releaseWriteBuffer(std::vector<uint8_t *> &buffs) {
            return this->releaseBuffer(buffs, &this->write_idx);
        }
    private:
        std::vector<std::vector<uint8_t *>> buffers;
        size_t buffer_len, elem_size;
        size_t write_idx, read_idx;
};
