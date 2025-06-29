#pragma once
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>

#include "Singleton.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

enum ErrorCodes
{
    Success = 0,
    Error_Json = 1001,
    PRCFailed = 1002,
};