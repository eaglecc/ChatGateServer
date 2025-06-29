#include "HttpConnection.h"
#include "LogicSystem.h"

HttpConnection::HttpConnection(tcp::socket socket) : _socket(std::move(socket))
{

}

void HttpConnection::Start()
{
    auto self = shared_from_this();
    http::async_read(_socket, _buffer, _request,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            try {
                if (ec) {
                    std::cout << "http read error: " << ec.message() << std::endl;
                    return;
                }
                boost::ignore_unused(bytes_transferred);
                self->HandleReq();
                self->CheckDeadline();
            } catch (const std::exception&)
            {
                std::cout << "Exception in HttpConnection::Start: " << std::endl;
            }
        });
}

// 十进制转十六进制
unsigned char ToHex(unsigned char c)
{
    return c > 9 ? c + 55 : c + 48;
}
// 十六进制转十进制
unsigned char FromHex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}
// 编码汉字
std::string UrlEncode(const std::string& str)
{
    std::string strTmp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') // 判断是否仅由数字和字母组成
        {
            strTmp += c;
        }
        else if (c == ' ') // 为空字符串
        {
            strTmp += '+';
        }
        else // 其他字符需要加%并且高四位和低四位转换成十六进制
        {
            strTmp += '%';
            strTmp += ToHex(c >> 4);
            strTmp += ToHex(c % 0x0F);
        }
    }
    return strTmp;
}
// 解码汉字
std::string UrlDecode(const std::string& str)
{
    std::string strTmp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = str[i];
        if (c == '%') // 如果是%则需要转换
        {
            if (i + 2 < length) // 确保后面有两个字符
            {
                unsigned char high = FromHex(str[i + 1]);
                unsigned char low = FromHex(str[i + 2]);
                strTmp += (high << 4) + low;
                i += 2; // 跳过后面的两个字符
            }
        }
        else if (c == '+') // 如果是+则转换为空格
        {
            strTmp += ' ';
        }
        else // 否则直接添加字符
        {
            strTmp += c;
        }
    }
    return strTmp;
}

void HttpConnection::PreParseGetParam() {
    // 提取 URI: get_test?key1=value1&key2=value2
    auto uri = _request.target();
    // 查找查询字符串的开始位置（即 '?' 的位置）  
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        _get_url = uri;
        return;
    }

    _get_url = uri.substr(0, query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    size_t pos = 0;
    while ((pos = query_string.find('&')) != std::string::npos) {
        auto pair = query_string.substr(0, pos);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码  
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }
    // 处理最后一个参数对（如果没有 & 分隔符）  
    if (!query_string.empty()) {
        size_t eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            _get_params[key] = value;
        }
    }
}

void HttpConnection::CheckDeadline()
{
    auto self = shared_from_this();
    _deadline.async_wait([self](beast::error_code ec) {
        if (!ec) {
            self->_socket.close(ec);
        }
    });
}

void HttpConnection::WriteResponse()
{
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(_socket, _response, [self](beast::error_code ec, std::size_t bytes_transferred) {
        self->_socket.shutdown(tcp::socket::shutdown_send, ec);
        self->_deadline.cancel();
    });
}

void HttpConnection::HandleReq()
{
    _response.version(_request.version());
    _response.keep_alive(false);
    if (_request.method() == http::verb::get)
    {
        PreParseGetParam(); // 解析URL，结果存在_get_url和_get_params中
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "Resource not found \r\n";
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server, "ChatGateServer");
        WriteResponse();
    }
    else if (_request.method() == http::verb::post)
    {
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "Resource not found \r\n";
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server, "ChatGateServer");
        WriteResponse();
    }
    else
    {
        _response.result(http::status::bad_request);
        _response.set(http::field::content_type, "text/plain");
        beast::ostream(_response.body()) << "Unsupported HTTP method \r\n";
        WriteResponse();
    }

}
