#include "Service/ApiKeyService.h"

#include "Repository/ApiKeyRepository.h"

std::string ApiKeyService::getKey(long long accountId, const std::string& provider)
{
    ApiKeyRepository repo;
    return repo.findByAccountAndProvider(accountId, provider);
}

bool ApiKeyService::saveKey(long long accountId, const std::string& provider, const std::string& apiKey)
{
    ApiKeyRepository repo;
    return repo.upsert(accountId, provider, apiKey);
}

json ApiKeyService::getMaskedKeys(long long accountId)
{
    ApiKeyRepository repo;
    return repo.findAllByAccount(accountId);
}
