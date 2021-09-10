///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021, STEREOLABS.
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

#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP

#include <chrono>

namespace sl_oc {
namespace tools {

/**
 * @brief Stop Timer used to measure time intervals
 *
 */
class StopWatch
{
public:
    /*!
     * \brief StopWatch constructor.
     *        Automatically starts time measurement.
     */
    StopWatch(){tic();}
    virtual ~StopWatch(){};

    /**
     * @brief Set the beginning time of the time measurement.
     */
    void tic()
    {
        mStartTime = std::chrono::steady_clock::now(); // Set the start time point
    }

    /**
     * @brief Calculates the seconds elapsed from the last tic.
     * @returns the seconds elapsed from the last @ref tic with microseconds resolution.
     */
    double toc()
    {
        auto now = std::chrono::steady_clock::now();
        double elapsed_usec = std::chrono::duration_cast<std::chrono::microseconds>(now - mStartTime).count();
        return elapsed_usec/1e6;
    }

private:
    std::chrono::steady_clock::time_point mStartTime;
};

} // namespace oc_tools
} // namespace sl_oc

#endif //STOPWATCH_HPP
