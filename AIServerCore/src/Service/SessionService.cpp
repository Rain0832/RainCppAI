#include "Service/SessionService.h"
#include "Repository/SessionRepository.h"
#include "Repository/MessageRepository.h"

json SessionService::listSessions(long long accountId)
{
    SessionRepository repo;
    return repo.findByAccount(accountId);
}

json SessionService::getHistory(const std::string& sessionId)
{
    MessageRepository repo;
    return repo.findBySession(sessionId);
}

bool SessionService::softDelete(const std::string& sessionId)
{
    SessionRepository repo;
    return repo.softDelete(sessionId);
}

bool SessionService::updateTitle(const std::string& sessionId, const std::string& title)
{
    SessionRepository repo;
    return repo.updateTitle(sessionId, title);
}

json SessionService::findById(const std::string& sessionId)
{
    SessionRepository repo;
    return repo.findById(sessionId);
}
