#pragma once

#include <string>

namespace common
{

/// SMTP mail configuration
struct MailConfig
{
    std::string host = "smtp.qq.com";
    int port = 465;
    std::string username;
    std::string password;
};

/// Result of a mail send operation
struct MailResult
{
    bool success = false;
    std::string message;
};

/// QQ SMTP mail sender via libcurl SMTPS
class MailSender
{
public:
    MailSender();
    explicit MailSender(const MailConfig& config);

    /// Send an email, returns success + message
    MailResult send(const std::string& to, const std::string& subject, const std::string& body);

    /// Load mail configuration from ConfigManager (section "mail")
    static MailConfig loadConfig();

private:
    MailConfig config_;
};

}  // namespace common
