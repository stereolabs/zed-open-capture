///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2026, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef ZED_OC_INTERNAL_HPP
#define ZED_OC_INTERNAL_HPP

// Private helpers shared by the modules of the library. Not installed.

#include "defines.hpp"

#include <array>
#include <mutex>

namespace sl_oc {

namespace detail {

/*!
 * \brief Print a message if allowed by the verbosity level
 * \param verbose the verbosity level chosen by the user
 * \param level the level of the message
 * \param tag the name of the module
 * \param msg the message
 */
inline void log(int verbose, VERBOSITY level, const char* tag, const std::string& msg)
{
    if( level==VERBOSITY::NONE || verbose<static_cast<int>(level) )
        return;

    const char* name = level==VERBOSITY::ERROR ? "ERROR" : (level==VERBOSITY::WARNING ? "WARNING" : "INFO");
    std::ostream& out = level==VERBOSITY::INFO ? std::cout : std::cerr;
    out << "[" << tag << "] " << name << ": " << msg << std::endl;
}

/*!
 * \brief State shared by a VideoCapture and a SensorCapture object when the frame synchronization is enabled.
 *
 * The object is owned by both sides with a shared pointer and neither side keeps a pointer to the other,
 * so the two objects can be closed and destroyed in any order.
 */
struct SyncLink
{
    static constexpr size_t PULSE_COUNT = 32;

    std::mutex mutex;
    std::array<uint64_t,PULSE_COUNT> pulseTs{}; //!< Timestamps of the latest frame sync signals seen by the sensors module
    uint64_t pulseTotal = 0;                    //!< Total number of signals stored

    /*!
     * \brief Called by the sensors module when the MCU reports a frame sync signal
     */
    void addPulse(uint64_t ts)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        pulseTs[pulseTotal % PULSE_COUNT] = ts;
        pulseTotal++;
    }

    /*!
     * \brief Search for the sync signal closest to a timestamp
     * \param target the timestamp
     * \param maxDistance the maximum distance accepted
     * \param ts receives the timestamp of the signal
     * \return true if a signal has been found
     */
    bool findPulse(uint64_t target, uint64_t maxDistance, uint64_t& ts)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const size_t count = pulseTotal<PULSE_COUNT ? static_cast<size_t>(pulseTotal) : PULSE_COUNT;
        uint64_t best = maxDistance;
        bool found = false;
        for( size_t i=0; i<count; i++ )
        {
            const uint64_t p = pulseTs[i];
            const uint64_t distance = p>target ? p-target : target-p;
            if( distance<=best )
            {
                best = distance;
                ts = p;
                found = true;
            }
        }
        return found;
    }
};

}

}

#endif // ZED_OC_INTERNAL_HPP
