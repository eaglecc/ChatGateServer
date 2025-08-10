#pragma once
#include "const.h"

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    // CServer类构造函数接受一个端口号，创建acceptor接受新到来的链接。
    CServer(boost::asio::io_context& ioc, unsigned short& port);
    void Start();

private:
    tcp::acceptor _acceptor; // 用于接受新的TCP连接
    net::io_context& _ioc;
    tcp::socket _socket;
};

