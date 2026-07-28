#pragma once

#include "nlohmann/json.hpp"
#include <cstdint>
#include <string>

using json = nlohmann::json;

class AdminRepository
{
public:
    /// Aggregated dashboard statistics from call_logs for today.
    json getDashboardStats();

    /// List all accounts with their status.
    json getUsers();

    /// Enable or disable a user account by id.
    bool toggleUserDisable(int64_t userId, int disabled);
};
