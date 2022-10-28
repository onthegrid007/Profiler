#include "profiler.h"
_SCM_CHILD_DEFINITIONS(Profiler::Session)

namespace Profiler {
    Session::Session(const std::string Name) :
        m_name(Name),
        m_mtx(new std::mutex()),
        m_sem(new Semaphore(0)),
        m_isRunning(false),
        m_thread(new std::thread([&](){ return this->Internal(); })) {
            m_filePath = m_name + "-Profile.json";
            // std::cout << "creating profile: " << m_name << std::endl;
            m_isRunning = true;
        }
        
    Session::Session(const std::string Name, const std::string FilePath) :
        m_name(Name),
        m_mtx(new std::mutex()),
        m_sem(new Semaphore(0)),
        m_filePath(FilePath),
        m_isRunning(false),
        m_thread(new std::thread([&](){ return this->Internal(); })) {
            // std::cout << "creating profile: " << m_name << std::endl;
            m_isRunning = true;
        }
    
    Session::~Session() {
        // std::cout << "destroying profile: " << m_name << std::endl;
    }
    
    void Session::BeginInternal() {
        while(!m_isRunning) std::_yield();
        // if(m_fileStream) m_fileStream->std::~ofstream();
        // std::cout << "session begin internal: " << m_name << std::endl;
        m_fileStream = new std::ofstream(m_filePath);
        WriteHeader();
    }
    
    void Session::WriteHeader() {
        m_fileStream->clear();
        *m_fileStream << std::setprecision(8) << std::fixed << "{\"otherData\": {},\"traceEvents\":[{}";
		m_fileStream->flush();
    }
    
    void Session::SaveResult(const Session::Result&& Result) {
        if(m_isRunning) {
            std::ThreadLock lock(*m_mtx);
            m_pendingResults.emplace_back(std::move(Result));
            m_sem->inc();
        }
   };

    void Session::WriteResult(const Session::Result& Result) {
        // std::cout << "write result: " << m_name << std::endl;
        *m_fileStream <<
        ",{" <<
        "\"cat\":\"function\"," <<
        "\"dur\":" << ADVClock::RuntimeCast<double>(Result.Elapsed, ADVClock::Precision::Microseconds) << ',' <<
        "\"name\":\"" << Result.Name << "\"," <<
        "\"ph\":\"X\"," <<
        "\"pid\":0," <<
        "\"tid\":" << Result.ID << "," <<
        "\"ts\":" << ADVClock::RuntimeCast<double>(Result.Start, ADVClock::Precision::Microseconds) <<
        "}";
        m_fileStream->flush();
    }
    
    void Session::WriteFooter() {
        *m_fileStream << "]}";
        m_fileStream->flush();
    }
    
    void Session::Internal() {
        BeginInternal();
        Result R = { "" };
        while(m_isRunning) {
            m_sem->waitFor([&, this](const std::I64 cVal, const std::I64 CInitVal){
                return (cVal != CInitVal) || (!m_isRunning);
            });
            std::ThreadLock lock(*m_mtx);
            if(m_isRunning) {
                if(m_pendingResults.size() > 0) InternalProcessFront(R);
            }
            else {
                // Flush
                while(m_pendingResults.size() > 0) InternalProcessFront(R);
                break;
            }
        }
        EndInternal();
    }
    
    void Session::InternalProcessFront(Session::Result& R) {
        R = { "" };
        R = std::move(m_pendingResults[0]);
        m_pendingResults.pop_front();
        WriteResult(R);
        m_sem->dec();
    }
    
    void Session::EndInternal() {
        WriteFooter();
        m_fileStream->close();
        delete m_fileStream;
        delete m_mtx;
    }
    
    void Session::BeginSession(const std::string SessionName) {
        CreateNewInstance(SessionName, SessionName);
    }
    
    void Session::BeginSession(const std::string SessionName, const std::string FilePath) {
        CreateNewInstance(SessionName, SessionName, FilePath);
    }
    
    void Session::SaveResult(const std::string SessionName, Session::Result&& Result) {
        GetInstanceByKey(SessionName, SessionName).SaveResult(std::move(Result));
    }
    
    void Session::EndSession(const std::string SessionName) {
        DeleteInstanceByKey(SessionName);
    };
    
    bool Session::SessionExists(const std::string SessionName) {
        return Exists(SessionName);
    };
    
    void Session::TerminateMain() {
        for(auto& session : CMap)
            Session::TerminateSession(session.first);
    }
    
    void Session::TerminateSession(const std::string SessionName) {
        auto& session = GetInstanceByKey(SessionName, SessionName);
        session.m_isRunning = false;
        session.m_sem->notify();
        session.m_thread->join();
    }
    
    Session::ProfileScope::ProfileScope(const std::string SessionName, const std::string Name) :
        m_sessionName(SessionName),
        m_result({ Name, std::this_thread::get_id(), GetInstanceByKey(SessionName, SessionName).m_timer.nowDur(), nanoseconds(0)}) {
        // std::cout << "Profiling scope: " << Name << " using session: " << SessionName << std::endl;
    }
    
    Session::ProfileScope::ProfileScope(Profiler::Session& session, const std::string Name) :
        m_sessionName(GetKeyByInstance(session)),
        m_result({ Name, std::this_thread::get_id(), session.m_timer.nowDur(), nanoseconds(0)}) { 
        // std::cout << "Profiling scope: " << Name << " using session: " << GetKeyByInstance(session) << std::endl; 
    }

    Session::ProfileScope::~ProfileScope() {
        const auto speed = GetInstanceByKey(m_sessionName, m_sessionName).m_timer.nowDur();
        // std::cout << "profiled and saving results: " << m_result.Name << std::endl;
        m_result.Elapsed = (speed - m_result.Start);
        Session::SaveResult(m_sessionName, std::move(m_result));
    }
}