/*
 *  Copyright (c) 2011 Bonelli Nicola <bonelli@antifork.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <string>
#include <thread>
#include <chrono>
#include <limits>
#include <cmath>
#include <atomic>

#include <pthread.h> // pthread_setaffinity_np

static inline void 
set_affinity(std::thread &t, int n) 
{
    if(t.get_id() == std::thread::id())
        throw std::runtime_error("thread not running");

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset); CPU_SET(n, &cpuset);

    auto pth = t.native_handle();
    if ( ::pthread_setaffinity_np(pth, sizeof(cpuset), &cpuset) != 0)
        throw std::runtime_error("pthread_setaffinity_np");
}


unsigned int hardware_concurrency()
{
    auto proc = []() -> int {
        std::ifstream cpuinfo("/proc/cpuinfo");
        return std::count(std::istream_iterator<std::string>(cpuinfo),
                          std::istream_iterator<std::string>(),
                          std::string("processor"));
    };
   
    auto hc = std::thread::hardware_concurrency();
    return hc ? hc : proc();
}


std::atomic_ulong p_pipe;
std::atomic_ulong c_pipe;

std::atomic_bool barrier;

const char * const BOLD  = "\E[1m";
const char * const RESET = "\E[0m";

enum class EShema
{
    Classic,
    PingPong,
    BigBang
};

int main( int argc, char* argv[] )
{
    const EShema schema = (argc<2)?EShema::Classic : argv[1][0]=='c'?EShema::Classic : argv[1][0]=='b'?EShema::BigBang : EShema::PingPong;
    const size_t trans  = (argc<3)?10000000 : std::atoll(argv[2]);
    const size_t ways   = (argc<4)?10       : std::atoll(argv[3]);
    const size_t cores  = hardware_concurrency();

    std::vector<double> TS(cores*cores);

    std::cout << "SpeedCore: running..." << std::endl;

    for(size_t i = 0; i < (cores-1); i++)
    {
        for(size_t j = i+1; j < cores; j++)
        {
            int n = i*cores+j;

            std::cout << "\rRunning test " << (i+1) << "/" << cores << " " << "/-\\|"[n&3] << std::flush;

            p_pipe.store(0);
            c_pipe.store(0);

            barrier.store(true, std::memory_order_release);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto classic_c =  [trans] {
                while (barrier.load(std::memory_order_acquire))
                {}
                for(unsigned int i = 1; i < trans; i++)
                {
                    p_pipe.store(i, std::memory_order_release);
                    while (c_pipe.load(std::memory_order_acquire) != i)
                    {}
                }};
            auto classic_p = [trans] {
                for(unsigned int i = 1; i < trans; i++)
                {
                    while(p_pipe.load(std::memory_order_acquire) != i)
                    {}
                    c_pipe.store(i,std::memory_order_release);
                }};

            // TODO: change pingpong to use SC/SP CLF queues
            auto pingpong_c =  [trans] {
                while (barrier.load(std::memory_order_acquire))
                {}
                for(unsigned int i = 1; i < trans; i++)
                {
                    p_pipe.store(i, std::memory_order_release);
                    while (c_pipe.load(std::memory_order_acquire) != i)
                    {}
                }};
            auto pingpong_p = [trans] {
                for(unsigned int i = 1; i < trans; i++)
                {
                    while(p_pipe.load(std::memory_order_acquire) != i)
                    {}
                    c_pipe.store(i,std::memory_order_release);
                }};

            std::thread c = schema==EShema::Classic ? std::thread(classic_c) : std::thread(pingpong_c);
            std::thread p = schema==EShema::Classic ? std::thread(classic_p) : std::thread(pingpong_p);

            set_affinity(c, i);
            set_affinity(p, j);

            auto begin = std::chrono::system_clock::now();
            
            barrier.store(false, std::memory_order_release);

            c.join();
            p.join();
            
            auto end = std::chrono::system_clock::now();

            TS[n] = static_cast<uint64_t>(trans)*1000/std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        }
    }

    auto it = std::max_element(TS.begin(), TS.end());
    auto max_ = *it;

    for(auto &elem : TS)
    {
        elem /= max_;
    }

    std::cout << "\nMax speed " << (max_*2) << " T/S (core " << 
                    std::distance(TS.begin(), it)/cores << " <-> " <<
                    std::distance(TS.begin(), it)%cores << ")" << std::endl;

    std::cout << "*\t";
    for(size_t i = 0; i < cores; i++) {
        std::cout << i << '\t';
    }
    std::cout << std::endl;

    for(size_t i = 0; i < cores; i++) {
        std::cout << i  << '\t';
        for(size_t j = 0; j < cores; j++)
        {
            auto & elem = TS[i * cores + j];
            if (elem != 0.0) {
                if (elem >  0.96)
                    std::cout << BOLD;
                std::cout << std::ceil(elem*100)/100 << RESET << '\t';
            }
            else
                std::cout << "-\t";

        }
        std::cout << std::endl;
    }

    return 0;
}
 
