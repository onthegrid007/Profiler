#ifndef PROFILER_H_
#define PROFILER_H_
// #include <string>
// #include <bitset>
// #include "vendor/portable-file-dialogs/portable-file-dialogs.h"

#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/STDExtras.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/Semaphore/vendor/Singleton/singleton_container_map.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/Semaphore/semaphore.h"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/ADVClock/advclock.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/FileUtilities.hpp"

namespace Profiler {
    class Session : public SingletonContainerMap<Profiler::Session> {
        public:
        struct Result {
            std::string Name;
            std::ThreadID ID;
            std::chrono::nanoseconds Start;
            std::chrono::nanoseconds Elapsed;
        };
        
        class ProfileScope {
            public:
            ProfileScope(const std::string SessionName, const std::string Name);
            ProfileScope(Session& session, const std::string Name);
            ~ProfileScope();
            
            private:
            std::string m_sessionName;
            Result m_result;
        };
        
        void SaveResult(const Session::Result&& Result);
        static void BeginSession(const std::string SessionName);
        static void BeginSession(const std::string SessionName, const std::string FilePath);
        static void SaveResult(const std::string SessionName, Session::Result&& Result);
        static void EndSession(const std::string SessionName);
        static bool SessionExists(const std::string SessionName);
        static void TerminateMain();
        static void TerminateSession(const std::string SessionName);
        
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
        void InternalProcessFront(Session::Result& R);
        void EndInternal();
        const std::string m_name;
        FileUtilities::ParsedPath m_filePath;
        Semaphore* m_sem;
        std::mutex* m_mtx;
        std::ofstream* m_fileStream;
        std::thread* m_thread;
        std::deque<Result> m_pendingResults;
        ADVClock m_timer;
        bool m_isRunning = false;
        bool m_isblocking;
        friend class ProfileScope;
    };
}
#endif