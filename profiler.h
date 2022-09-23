#ifndef PROFILER_H_
#define PROFILER_H_

#include <condition_variable>
#include <deque>
#include "vendor/STDExtras/STDExtras.hpp"
#include "vendor/STDExtras/ADVClock/advclock.hpp"
#include "vendor/STDExtras/vendor/Singleton/singleton_container_map.hpp"

using namespace std::this_thread;
using namespace std::chrono;

namespace Profiler {    
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