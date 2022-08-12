#ifndef PROFILER_H_
#define PROFILER_H_

#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <fstream>
#include <deque>
#include "vendor/std_extras/std_extras.hpp"
#include "vendor/std_extras/advclock/advclock.hpp"
#include "vendor/std_extras/vendor/singleton/singleton.hpp"

using namespace std::this_thread;
using namespace std::chrono;

using Dur = duration<double, std::nano>;
using Elapsed = nanoseconds;
using ThreadLock = std::lock_guard<std::mutex>;
using ThreadID = std::thread::id;

class Profiler : public Singleton<Profiler> {
    public:
    struct Session {
        public:
        struct Results {
            Dur Start;
            Elapsed ElapsedTime;
            ThreadID ID;
        };
        private:
        Session& BeginSession(std::string SessionName, std::string FilePath);
        std::string SessionName;
        std::string FilePath;
        std::mutex Mutex;
        std::ofstream FileStream;
        std::thread Thread;
        ADVClock Timer;
        void EndSession();
        friend class Profiler;
    };
    Session& BeginSession(std::string SessionName, std::string FilePath);
    void SaveResults(Session::Results& Results);
    void EndSession(std::string SessionName);
    private:
    Profiler();
    _SINGLETON_CHILD_DECLORATIONS(Profiler)
};

#endif