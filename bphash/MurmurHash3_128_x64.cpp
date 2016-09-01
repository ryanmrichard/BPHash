/*! \file
 * \brief MurmurHash3_128_x64 hash implementation (source)
 */

#include "bphash/MurmurHash3_128_x64.hpp"

//////////////////////////////////////////
// Some small functions for the hash algo
//////////////////////////////////////////
static inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}


static inline uint64_t fmix64 ( uint64_t k )
{
  k ^= k >> 33;
  k *= (0xff51afd7ed558ccdLLU);
  k ^= k >> 33;
  k *= (0xc4ceb9fe1a85ec53LLU);
  k ^= k >> 33;

  return k;
}



namespace bphash {
namespace detail {



////////////////////////////////
// Public functions
////////////////////////////////

MurmurHash3_128_x64::MurmurHash3_128_x64(void)
{
    reset();
}


void MurmurHash3_128_x64::reset(void)
{
    h1_ = h2_ = 0;
    len_ = 0;
    std::fill(buffer_.begin(), buffer_.end(), 0);
    nbuffer_ = 0;
}


void MurmurHash3_128_x64::update(void const * data, size_t nbytes)
{
    if(nbytes == 0)
        return; // got nothing to do

    // cast to an array of bytes
    const uint8_t * data_conv = static_cast<const uint8_t*>(data);

    // Are we doing this in-place (ie, without copying to a temporary butter)?
    // This is true if we don't have leftovers stored already
    if(nbuffer_ == 0)
        update_inplace_(data_conv, nbytes);
    else
        update_via_buffer_(data_conv, nbytes);
}


void MurmurHash3_128_x64::pad_out(void)
{
    if(nbuffer_ != 0)
    {
        std::fill(buffer_.begin() + nbuffer_,
                  buffer_.end(),
                  0);
        update_block_(buffer_.data());
        nbuffer_ = 0;
    }
}


HashValue MurmurHash3_128_x64::finalize(void)
{
    // If we have any left over, we have to do that
    if(nbuffer_ > 0)
    {
        const uint8_t * tail = static_cast<const uint8_t*>(buffer_.data());

        uint64_t k1 = 0;
        uint64_t k2 = 0;

        switch(nbuffer_ & 15)
        {
            case 15: k2 ^= (static_cast<uint64_t>(tail[14])) << 48;
            case 14: k2 ^= (static_cast<uint64_t>(tail[13])) << 40;
            case 13: k2 ^= (static_cast<uint64_t>(tail[12])) << 32;
            case 12: k2 ^= (static_cast<uint64_t>(tail[11])) << 24;
            case 11: k2 ^= (static_cast<uint64_t>(tail[10])) << 16;
            case 10: k2 ^= (static_cast<uint64_t>(tail[ 9])) << 8;
            case  9: k2 ^= (static_cast<uint64_t>(tail[ 8])) << 0;
                     k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2_ ^= k2;

            case  8: k1 ^= (static_cast<uint64_t>(tail[ 7])) << 56;
            case  7: k1 ^= (static_cast<uint64_t>(tail[ 6])) << 48;
            case  6: k1 ^= (static_cast<uint64_t>(tail[ 5])) << 40;
            case  5: k1 ^= (static_cast<uint64_t>(tail[ 4])) << 32;
            case  4: k1 ^= (static_cast<uint64_t>(tail[ 3])) << 24;
            case  3: k1 ^= (static_cast<uint64_t>(tail[ 2])) << 16;
            case  2: k1 ^= (static_cast<uint64_t>(tail[ 1])) << 8;
            case  1: k1 ^= (static_cast<uint64_t>(tail[ 0])) << 0;
                     k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1_ ^= k1;
        };
    }

    // How much we've done altogether
    len_ += static_cast<size_t>(nbuffer_);

    // Last steps of the hash
    h1_ ^= len_; h2_ ^= len_;

    h1_ += h2_;
    h2_ += h1_;

    h1_ = fmix64(h1_);
    h2_ = fmix64(h2_);

    h1_ += h2_;
    h2_ += h1_;

    // Create the hash object and return
    return HashValue{ static_cast<uint8_t>(h1_),
                      static_cast<uint8_t>(h1_ >> 8),
                      static_cast<uint8_t>(h1_ >> 16),
                      static_cast<uint8_t>(h1_ >> 24),
                      static_cast<uint8_t>(h1_ >> 32),
                      static_cast<uint8_t>(h1_ >> 40),
                      static_cast<uint8_t>(h1_ >> 48),
                      static_cast<uint8_t>(h1_ >> 56),
                      static_cast<uint8_t>(h2_),
                      static_cast<uint8_t>(h2_ >> 8),
                      static_cast<uint8_t>(h2_ >> 16),
                      static_cast<uint8_t>(h2_ >> 24),
                      static_cast<uint8_t>(h2_ >> 32),
                      static_cast<uint8_t>(h2_ >> 40),
                      static_cast<uint8_t>(h2_ >> 48),
                      static_cast<uint8_t>(h2_ >> 56) };
}




////////////////////////////////
// Private member functions
////////////////////////////////
void MurmurHash3_128_x64::update_block_(uint8_t const * data)
{
    // This function only does an entire 16-byte buffer
    // (that is stored as private member buffer_)
    const uint64_t * block64 = reinterpret_cast<const uint64_t *>(data);

    uint64_t k1 = block64[0];
    uint64_t k2 = block64[1];

    k1 *= c1;
    k1  = rotl64(k1, 31);
    k1 *= c2;

    h1_ ^= k1;
    h1_ = rotl64(h1_, 27);
    h1_ += h2_;
    h1_ = h1_*5+0x52dce729;

    k2 *= c2;
    k2  = rotl64(k2, 33);
    k2 *= c1;
    h2_ ^= k2;

    h2_ = rotl64(h2_, 31);
    h2_ += h1_;
    h2_ = h2_*5+0x38495ab5;

    // update how much we've actually hashed
    len_ += 16;
}


void MurmurHash3_128_x64::update_inplace_(uint8_t const * data, size_t nbytes)
{
    while(nbytes >= 16)
    {
        // Compute the hash for this block
        update_block_(data);

        // Move the data pointer
        data += 16;

        // How many bytes are left to do?
        // Underflow won't happen because this loop won't run
        // with nbytes < 16
        nbytes -= 16;
    }

    // Leave any remainder in the main buffer
    // (we already know that nbytes < 16, or else
    // the while loop would have kept going)
    if(nbytes != 0)
    {
        std::copy(data, data + nbytes, buffer_.begin());
        nbuffer_ = nbytes;
    }
}


void MurmurHash3_128_x64::update_via_buffer_(uint8_t const * data, size_t nbytes)
{
    // Temporary buffer
    // Reason: If an instance of this class is created on the heap, then
    // buffer_ will be on the heap. This leads to a noticible slowdown
    // on some systems
    std::array<uint8_t, 16> tmpbuf = buffer_;

    // Amount of space left in the buffer?
    size_t nbytes_avail = 16 - nbuffer_;

    // How much of data should we actually copy?
    size_t tocopy = std::min(nbytes, nbytes_avail);

    // copy it
    std::copy(data, data + tocopy, tmpbuf.begin() + nbuffer_);

    // how much do we have in the buffer now?
    nbuffer_ += tocopy;

    while(nbuffer_ == 16)
    {
        // Compute the hash for this block
        update_block_(tmpbuf.data());

        // tocopy represents how many we just copied to the buffer
        // and then hashed.

        // Move the data pointer
        data += tocopy;

        // How many bytes are left to do?
        // This should not underflow since
        // tocopy = std::min(nbytes, ...)
        nbytes -= tocopy;

        // we have the entire buffer available again
        nbytes_avail = 16;

        // how much are we going to copy now?
        tocopy = std::min(nbytes, nbytes_avail);

        // copy it to the buffer
        std::copy(data, data + tocopy, tmpbuf.begin());

        // how much is stored in the buffer is how much
        // we copied to it
        nbuffer_ = tocopy;
    }

    // Leave any remainder/tail in the main buffer for next time
    buffer_ = tmpbuf;
}


} // close namespace detail
} // close namespace bphash

