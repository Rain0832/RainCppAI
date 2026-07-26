#include "Common/Mail/MailSender.h"
#include "Common/Config/ConfigManager.h"

#include <curl/curl.h>
#include <cstring>
#include <sstream>

namespace common {

MailSender::MailSender()
    : config_(loadConfig())
{}

MailSender::MailSender(const MailConfig& config)
    : config_(config)
{}

MailConfig MailSender::loadConfig()
{
    auto& cfg = ConfigManager::instance();
    MailConfig mc;
    mc.host     = cfg.get("mail.host", "smtp.qq.com");
    mc.port     = cfg.getInt("mail.port", 465);
    mc.username = cfg.get("mail.username", "");
    mc.password = cfg.get("mail.password", "");
    return mc;
}

namespace {

static size_t readPayload(void* ptr, size_t size, size_t nmemb, void* userp)
{
    auto* stream = static_cast<std::stringstream*>(userp);
    size_t toRead = size * nmemb;
    if (stream->rdbuf()->in_avail() == 0)
        return 0;
    stream->read(static_cast<char*>(ptr), static_cast<std::streamsize>(toRead));
    return static_cast<size_t>(stream->gcount());
}

}

MailResult MailSender::send(const std::string& to, const std::string& subject, const std::string& body)
{
    if (config_.username.empty() || config_.password.empty())
        return {false, "Mail not configured: username or password is empty"};

    std::string from = config_.username;
    std::stringstream payload;

    payload << "From: " << from << "\r\n";
    payload << "To: " << to << "\r\n";
    payload << "Subject: " << subject << "\r\n";
    payload << "MIME-Version: 1.0\r\n";
    payload << "Content-Type: text/plain; charset=UTF-8\r\n";
    payload << "Content-Transfer-Encoding: 8bit\r\n";
    payload << "\r\n";
    payload << body << "\r\n";

    std::string url = "smtps://" + config_.host + ":" + std::to_string(config_.port);

    CURL* curl = curl_easy_init();
    if (!curl)
        return {false, "Failed to initialize curl"};

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, to.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_USERNAME, config_.username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, config_.password.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readPayload);
    curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK)
        return {true, "Email sent successfully"};
    else
        return {false, std::string("CURL error: ") + curl_easy_strerror(res)};
}

}
