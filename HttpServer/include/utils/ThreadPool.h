#pragma once
// Compatibility alias: ThreadPool extracted to Common/Threading/
// Existing code using http::ThreadPool continues to work unchanged.
#include "Common/Threading/ThreadPool.h"
namespace http { using ThreadPool = common::ThreadPool; }
