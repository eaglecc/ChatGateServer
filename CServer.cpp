#include "CServer.h"
#include "HttpConnection.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
    : _ioc(ioc),
    _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
    _socket(ioc)
{
}

void CServer::Start()
{
    auto self = shared_from_this();
    _acceptor.async_accept(_socket,
        [self](beast::error_code ec) {
            try {
                if (ec) {
                    std::cout << "Accept error: " << ec.message() << std::endl;
                    self->Start(); 
                    return;
                }
                // 有新的连接，创建HttpConnection对象来处理
                std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
                
                // 重新创建socket并继续接受新连接
                self->_socket = tcp::socket(self->_ioc);
                self->Start();
            }
            catch (const std::exception& e) {
                std::cout << "Exception in CServer::Start: " << e.what() << std::endl;
                // 即使出现异常也要继续接受新连接
                self->_socket = tcp::socket(self->_ioc);
                self->Start();
            }
        });
}