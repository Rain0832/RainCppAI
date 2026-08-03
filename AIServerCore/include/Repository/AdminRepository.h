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

    /// List all accounts with their status and invite code used.
    json getUsers();

    /// Enable or disable a user account by id.
    bool toggleUserDisable(int64_t userId, int disabled);

    /// List all invite codes with usage stats.
    json getInviteCodes();

    /// Create a new invite code. Returns the created code json.
    json createInviteCode(int64_t createdBy, int maxUses, bool isAdmin);

    /// Toggle invite code disabled status.
    bool toggleInviteCode(int64_t codeId, int disabled);
};
