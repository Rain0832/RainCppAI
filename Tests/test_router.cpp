#include <gtest/gtest.h>
#include "router/Router.h"
using namespace http::router;

TEST(RouterTest, ExactMatch)
{
    Router r;
    bool called = false;
    r.registerCallback(HttpRequest::kGet, "/test", [&](const HttpRequest&, HttpResponse*) { called = true; });
    HttpRequest req; req.setMethod(HttpRequest::kGet); req.setPath("/test");
    HttpResponse resp;
    EXPECT_TRUE(r.route(req, &resp));
    EXPECT_TRUE(called);
}

TEST(RouterTest, RegexMatch)
{
    Router r;
    std::string captured;
    r.addRegexCallback(HttpRequest::kGet, "/css/:file", [&](const HttpRequest& req, HttpResponse*) {
        captured = req.getPathParameter("file");
    });
    HttpRequest req; req.setMethod(HttpRequest::kGet); req.setPath("/css/style.css");
    HttpResponse resp;
    EXPECT_TRUE(r.route(req, &resp));
    EXPECT_EQ(captured, "style.css");
}
