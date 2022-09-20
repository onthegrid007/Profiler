#ifndef PROFILER_H_
#define PROFILER_H_

#include <thread>
#include <condition_variable>
#include <fstream>
#include <deque>
#include "vendor/FileUtilities/vendor/STDExtras/std_extras.hpp"
#include "vendor/FileUtilities/vendor/STDExtras/advclock/advclock.hpp"
#include "vendor/FileUtilities/vendor/STDExtras/vendor/Singleton/singleton_container_map.hpp"

using namespace std::this_thread;
using namespace std::chrono;

// using Dur = duration<double, std::nano>;
// using Elapsed = nanoseconds;

namespace Profiler {
    template <size_t N>
    struct rValStr {
        char data[N];
    };
    template <size_t N, size_t K>
    constexpr auto RemoveStringContents(const char(&expr)[N], const char(&remove)[K]) {
        rValStr<N> result = {};
        size_t srcIdx = 0;
        size_t dstIdx = 0;
        while(srcIdx < N) {
            size_t matchIdx = 0;
            while(matchIdx < K - 1 && srcIdx + matchIdx < N - 1 && expr[srcIdx + matchIdx] == remove[matchIdx])
                matchIdx++;
            if(matchIdx == (K - 1))
                srcIdx += matchIdx;
            result.data[dstIdx++] = expr[srcIdx] == '"' ? '\'' : expr[srcIdx];
            srcIdx++;
        }
        return result;
    }
    
    class Session : public SingletonContainerMap<Session> {
        public:
        struct Result {
            std::string Name;
            std::ThreadID ID;
            nanoseconds Start;
            nanoseconds Elapsed;
        };
        
        class ProfileScope {
            public:
            ProfileScope(const std::string SessionName, const std::string Name);
            ~ProfileScope();
            
            private:
            std::string m_sessionName;
            Result m_result;
        };
        
        void SaveResult(const Session::Result&& Result);
        static void BeginSession(const std::string SessionName, const std::string FilePath);
        static void SaveResult(const std::string SessionName, Session::Result&& Result);
        static void EndSession(const std::string SessionName);
        static bool SessionExists(const std::string SessionName);
        
        private:
        _SCM_CHILD_DECLORATIONS(Session)
        Session(const std::string Name);
        Session(const std::string Name, const std::string FilePath);
        ~Session();
        void BeginInternal();
        void Internal();
        void WriteHeader();
        void WriteResult(const Session::Result& Result);
        void WriteFooter();
        void EndInternal();
        const std::string m_name;
        const std::string m_filePath;
        std::condition_variable m_cv;
        std::mutex m_mtx;
        std::ofstream m_fileStream;
        std::thread m_thread;
        std::deque<Result> m_pendingResults;
        ADVClock m_timer;
        bool m_isRunning;
        bool m_isblocking;
        friend class ProfileScope;
    };
}
#endif