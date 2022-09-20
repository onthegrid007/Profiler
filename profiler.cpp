#include "profiler.h"
_SCM_CHILD_DEFINITIONS(Profiler::Session)

namespace Profiler {
    Session::Session(const std::string Name) :
        m_filePath(Name + "-Profile.json"),
        m_fileStream(m_filePath),
        m_isRunning(true),
        m_thread([&](){ return this->Internal(); }) {}
        
    Session::Session(const std::string Name, const std::string FilePath) :
        m_filePath(FilePath),
        m_fileStream(m_filePath),
        m_isRunning(true),
        m_thread([&](){ return this->Internal(); }) {}
    
    Session::~Session() {
        m_isRunning = false;
        m_cv.notify_all();
        m_thread.join();
        m_fileStream.flush();
        m_fileStream.close();
    }
    
    void Session::BeginInternal() {
        WriteHeader();
    }
    
    void Session::WriteHeader() {
        m_fileStream.clear();
        m_fileStream << std::setprecision(8) << std::fixed << "{\"otherData\": {},\"traceEvents\":[{}";
		m_fileStream.flush();
    }
    
    void Session::SaveResult(const Session::Result&& Result) {
        std::CVThreadLock lock(m_mtx);
        m_pendingResults.emplace_back(std::move(Result));
        m_cv.notify_all();
   };

    void Session::WriteResult(const Session::Result& Result) {
        m_fileStream <<
        ",{" <<
        "\"cat\":\"function\"," <<
        "\"dur\":" << ADVClock::RuntimeCast<double>(Result.Elapsed, ADVClock::Precision::Microseconds) << ',' <<
        "\"name\":\"" << Result.Name << "\"," <<
        "\"ph\":\"X\"," <<
        "\"pid\":0," <<
        "\"tid\":" << Result.ID << "," <<
        "\"ts\":" << ADVClock::RuntimeCast<double>(Result.Start, ADVClock::Precision::Microseconds) <<
        "}";
        m_fileStream.flush();
    }
    
    void Session::WriteFooter() {
        m_fileStream << "]}";
        m_fileStream.flush();
    }
    
    void Session::Internal() {
        BeginInternal();
        while(m_isRunning) {
            Result&& R = {};
            {
                std::CVThreadLock lock(m_mtx);
                m_cv.wait(lock, [&] { return (m_pendingResults.size() > 0) || (!m_isRunning); });
                if(m_isRunning){
                    const bool tryL = m_mtx.try_lock();
                    R = std::move(m_pendingResults[0]);
                    m_pendingResults.pop_front();
                    if(tryL) m_mtx.unlock();
                }
            }
            if(m_isRunning) WriteResult(R);
        }
        EndInternal();
    }
    
    void Session::EndInternal() {
        WriteFooter();
    }
    
    void Session::BeginSession(const std::string SessionName, const std::string FilePath) {
        SCM::CreateNewInstance(SessionName, SessionName, FilePath);
    }
    
    void Session::SaveResult(const std::string SessionName, Session::Result&& Result) {
        SCM::GetInstanceByKey(SessionName, SessionName, SessionName + "-Profile.json").SaveResult(std::move(Result));
    }
    
    void Session::EndSession(const std::string SessionName) {
        SCM::DeleteInstanceByKey(SessionName);
    };
    
    bool Session::SessionExists(const std::string SessionName) {
        return SCM::Exists(SessionName);
    };
    
    Session::ProfileScope::ProfileScope(const std::string SessionName, const std::string Name) :
        m_sessionName(SessionName),
        m_result({ Name, std::this_thread::get_id(), (SessionExists(SessionName) ? SCM::GetInstanceByKey(SessionName, SessionName) : SCM::CreateNewInstance(SessionName, SessionName, SessionName + "-Profile.json")).m_timer.nowDur(), nanoseconds(0)}) { std::cout << "Profiling scope: " << Name << " using session: " << SessionName << std::endl; }
    
    Session::ProfileScope::~ProfileScope() {
        const auto speed = SCM::GetInstanceByKey(m_sessionName, m_sessionName).m_timer.nowDur();
        std::cout << "profiled and saving results: " << m_result.Name << std::endl;
        m_result.Elapsed = (speed - m_result.Start);
        Session::SaveResult(m_sessionName, std::move(m_result));
    }
}