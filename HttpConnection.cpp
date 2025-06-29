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

// ʮ����תʮ������
unsigned char ToHex(unsigned char c)
{
    return c > 9 ? c + 55 : c + 48;
}
// ʮ������תʮ����
unsigned char FromHex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}
// ���뺺��
std::string UrlEncode(const std::string& str)
{
    std::string strTmp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') // �ж��Ƿ�������ֺ���ĸ���
        {
            strTmp += c;
        }
        else if (c == ' ') // Ϊ���ַ���
        {
            strTmp += '+';
        }
        else // �����ַ���Ҫ��%���Ҹ���λ�͵���λת����ʮ������
        {
            strTmp += '%';
            strTmp += ToHex(c >> 4);
            strTmp += ToHex(c % 0x0F);
        }
    }
    return strTmp;
}
// ���뺺��
std::string UrlDecode(const std::string& str)
{
    std::string strTmp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = str[i];
        if (c == '%') // �����%����Ҫת��
        {
            if (i + 2 < length) // ȷ�������������ַ�
            {
                unsigned char high = FromHex(str[i + 1]);
                unsigned char low = FromHex(str[i + 2]);
                strTmp += (high << 4) + low;
                i += 2; // ��������������ַ�
            }
        }
        else if (c == '+') // �����+��ת��Ϊ�ո�
        {
            strTmp += ' ';
        }
        else // ����ֱ������ַ�
        {
            strTmp += c;
        }
    }
    return strTmp;
}

void HttpConnection::PreParseGetParam() {
    // ��ȡ URI: get_test?key1=value1&key2=value2
    auto uri = _request.target();
    // ���Ҳ�ѯ�ַ����Ŀ�ʼλ�ã��� '?' ��λ�ã�  
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
            key = UrlDecode(pair.substr(0, eq_pos)); // ������ url_decode ����������URL����  
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }
    // �������һ�������ԣ����û�� & �ָ�����  
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
        PreParseGetParam(); // ����URL���������_get_url��_get_params��
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
