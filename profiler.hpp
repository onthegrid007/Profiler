#ifndef PROFILER_H_
#define PROFILER_H_

#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/STDExtras.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/threadpool.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/Semaphore/semaphore.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/Semaphore/vendor/Singleton/nonmoveable.h"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/vendor/STDExtras/vendor/ThreadPool/vendor/ADVClock/advclock.hpp"
#include "vendor/FPNBits/vendor/DynamicDLL/vendor/FileUtilities/FileUtilities.hpp"

namespace Profiler {
    class Session : public NonCopyable, public NonMovable {
        public:
        struct Result {
            std::string Name;
            PlatformThread::IDType ID;
            std::chrono::nanoseconds Start;
            std::chrono::nanoseconds Elapsed;
        };
        
        inline static std::unordered_map<std::string, Session*> _Sessions{};
        inline static std::mutex _SMTX{};
        
        static const bool SessionExists(const std::string SessionName) {
            return ((_Sessions.find(SessionName) != _Sessions.end()) && !_Sessions.empty());
        }
        
        static Session& BeginSession(const std::string SessionName) {
            std::ThreadLock lock(_SMTX);
            if(SessionExists(SessionName))
                return *(_Sessions[SessionName]);
            return *(_Sessions[SessionName] = std::move(new Session(SessionName)));
        }
        
        static Session& BeginSession(const std::string SessionName, const std::string FilePath) {
            std::ThreadLock lock(_SMTX);
            if(SessionExists(SessionName))
                return *(_Sessions[SessionName]);
            return *(_Sessions[SessionName] = std::move(new Session(SessionName, FilePath)));
        }
        
        static Session& GetSession(const std::string SessionName) {
            return BeginSession(SessionName);
        }
        
        static void SaveResult(const std::string SessionName, const Session::Result&& Result) {
            Session& self{GetSession(SessionName)};
            if(self.m_isRunning) {
                std::ThreadLock lock(self.m_mtx);
                self.m_pendingResults.emplace_back(std::move(Result));
                self.m_sem.inc();
            }
        }
        
        static void EndSession(const std::string SessionName) {
            std::ThreadLock lock(_SMTX);
            if(!SessionExists(SessionName)) return;
            const auto self{_Sessions.find(SessionName)};
            self->second->~Session();
            _Sessions.erase(self);
        }
        
        class ProfileScope {
            private:
            const std::string m_sessionName;
            Result m_result;
            
            public:
            ProfileScope(const std::string SessionName, const std::string Name) :
                m_sessionName(SessionName),
                m_result{Name, PlatformThread::ID(), GetSession(SessionName).m_timer.nowDur(), {}} {}
            
            ~ProfileScope() {
                const auto speed{GetSession(m_sessionName).m_timer.nowDur()};
                m_result.Elapsed = speed - m_result.Start;
                SaveResult(m_sessionName, std::move(m_result));
            }
        };
        
        private:
        const std::string m_name;
        FileUtilities::ParsedPath m_filePath;
        Semaphore m_sem{0};
        std::mutex m_mtx;
        PlatformThread* m_thread;
        std::deque<Result> m_pendingResults;
        ADVClock m_timer;
        bool m_isRunning{true};
        friend class ProfileScope;
        
        void Internal() {
            Result R{};
            std::ofstream S(m_filePath);
            if(!(S.is_open())) return;
            S.clear();
            S << std::setprecision(8) << std::fixed << "{\"otherData\": {},\"traceEvents\":[{}";
		    S.flush();
            while(m_isRunning) {
                m_sem.waitFor([&, this](const std::I64 cVal, const std::I64 CInitVal){
                    return (cVal != CInitVal) || (!m_isRunning);
                });
                std::ThreadLock lock(m_mtx);
                while(m_pendingResults.size() > 0) {
                    R = {};
                    R = std::move(m_pendingResults[0]);
                    m_pendingResults.pop_front();
                    S <<
                    ",{" <<
                    "\"cat\":\"function\"," <<
                    "\"dur\":" << ADVClock::ElapsedRuntimeCast<double>(R.Elapsed, ADVClock::Precision::MicroS) << ',' <<
                    "\"name\":\"" << R.Name << "\"," <<
                    "\"ph\":\"X\"," <<
                    "\"pid\":0," <<
                    "\"tid\":" << R.ID << "," <<
                    "\"ts\":" << ADVClock::ElapsedRuntimeCast<double>(R.Start, ADVClock::Precision::MicroS) <<
                    "}";
                    S.flush();
                    m_sem.dec();
                }
            }
            S << "]}";
            S.flush();
            S.close();
        }
        
        Session(const std::string Name) :
            m_name(Name),
            m_filePath(Name + ".profile", FileUtilities::ParsedPath::RELATIVE{}),
            m_thread(new PlatformThread([this](){ return this->Internal(); })) {}
        
        Session(const std::string Name, const std::string FilePath) :
            m_name(Name),
            m_filePath(FilePath, FileUtilities::ParsedPath::ABS{}),
            m_thread(new PlatformThread([this](){ return this->Internal(); })) {}
        
        ~Session() {
            m_isRunning = false;
            m_sem.spinAll();
            if(m_thread) {
                if(m_thread->joinable()) m_thread->join();
                m_thread->~PlatformThread();
                delete m_thread;
                m_thread = 0;
            }
        }
    };
}
#endif