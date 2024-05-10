/*
*   BSD 3-Clause License, see file labled 'LICENSE' for the full License.
*   Copyright (c) 2024, Peter Ferranti
*   All rights reserved.
*/

#ifndef PROFILER_HPP_
#define PROFILER_HPP_

#include "vendor/ThreadPool/threadpool.hpp"
#include "vendor/ThreadPool/vendor/Semaphore/semaphore.hpp"
#include "vendor/ThreadPool/vendor/Semaphore/vendor/Singleton/nonmoveable.h"
#include "vendor/ThreadPool/vendor/ADVClock/advclock.hpp"
#include <fstream>

namespace Profiler {
    class Session : public NonCopyable, public NonMovable {
        public:
        typedef std::lock_guard<std::mutex> Lock;
        struct Result {
            std::string Name;
            PlatformThread::IDType ID;
            std::chrono::nanoseconds Start;
            std::chrono::nanoseconds Elapsed;
        };
        
        inline static std::unordered_map<std::string, Session*> _Sessions{};
        inline static std::mutex _SMTX{};
        
        static const bool SessionExists(const std::string SessionName) {
            return ((_Sessions.find(SessionName) != _Sessions.end()));
        }
        
        static Session& BeginSession(const std::string SessionName) {
            Lock lock(_SMTX);
            if(SessionExists(SessionName))
                return *(_Sessions[SessionName]);
            return *(_Sessions[SessionName] = std::move(new Session(SessionName)));
        }
        
        static Session& BeginSession(const std::string SessionName, const std::string FilePath) {
            Lock lock(_SMTX);
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
                Lock lock(self.m_mtx);
                self.m_pendingResults.emplace_back(std::move(Result));
                self.m_sem.inc();
            }
        }
        
        static void EndSession(const std::string SessionName) {
            Lock lock(_SMTX);
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
        ADVClock m_timer;
        const std::string m_name;
        bool m_isRunning;
        const std::string m_filePath;
        Semaphore m_sem;
        std::mutex m_mtx;
        PlatformThread* m_thread;
        std::deque<Result> m_pendingResults;
        friend class ProfileScope;
        
        void Internal() {
            Result R{};
            std::ofstream S(m_filePath);
            if(!(S.is_open())) throw std::runtime_error("Could not open profiler output!");
            S.clear();
            S << std::setprecision(8) << std::fixed << "{\"otherData\": {},\"traceEvents\":[{}";
		    S.flush();
            while(m_isRunning) {
                m_sem.waitFor([&, this](const std::uint64_t cVal, const std::uint64_t CInitVal){
                    return (cVal != CInitVal) || (!m_isRunning);
                });
                Lock lock(m_mtx);
                while(m_pendingResults.size() > 0) {
                    // R = {};
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
            m_isRunning(true),
            m_filePath(Name + ".profile"),
            m_sem(0),
            m_thread(new PlatformThread([this](){ return this->Internal(); })) {}
        
        Session(const std::string Name, const std::string FilePath) :
            m_name(Name),
            m_isRunning(true),
            m_filePath(FilePath),
            m_sem(0),
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