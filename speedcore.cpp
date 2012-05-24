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
    auto proc = []() {
        std::ifstream cpuinfo("/proc/cpuinfo");
        return std::count(std::istream_iterator<std::string>(cpuinfo),
                          std::istream_iterator<std::string>(),
                          std::string("processor"));
    };
   
    return std::thread::hardware_concurrency() ? : proc();
}

std::atomic_long p_pipe;
std::atomic_long c_pipe;

int
main(int argc, char *argv[])
{
    const size_t core = hardware_concurrency();

    std::vector<double> elapsed(core*core, std::numeric_limits<double>::max());

    for(size_t i = 0; i < (core-1); i++)
    {
        for(size_t j = i+1; j < core; j++)
        {
            int n = i*core +j;

            std::cout << "SpeedCore " << "/-\\|"[n&3] << '\r' << std::flush;

            p_pipe.store(0);
            c_pipe.store(0);

            std::thread c ([] {
                for(long i = 1; i < 10000000; i++)
                {
                    p_pipe.store(i);
                    while (c_pipe.load() != i)
                    {}
                }});

            std::thread p ([] {
                for(long i = 1; i < 10000000; i++)
                {
                    while(p_pipe.load() != i)
                    {}
                    c_pipe.store(i);
                }});

            set_affinity(c, i);
            set_affinity(p, j);

            auto begin = std::chrono::system_clock::now();

            c.join();
            p.join();
            
            auto end = std::chrono::system_clock::now();

            elapsed[n] = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        }
    }

    auto min_ = * std::min_element(elapsed.begin(), elapsed.end());
    for(auto &elem : elapsed)
    {
        if (elem != std::numeric_limits<double>::max())
            elem /= min_;
    }

    std::cout << std::endl << "*\t";
    for(size_t i = 0; i < core; i++) {
        std::cout << i << '\t';
    }
    std::cout << std::endl;

    for(size_t i = 0; i < core; i++) {
        std::cout << i  << '\t';
        for(size_t j = 0; j < core; j++)
        {
            auto & elem = elapsed[i * core + j];
            if (elem != std::numeric_limits<double>::max())
                std::cout << std::ceil(elem*100)/100 << '\t';
            else
                std::cout << "-\t";

        }
        std::cout << std::endl;
    }

    return 0;
}
 
