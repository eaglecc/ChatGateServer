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
                    self->Start(); return;
                }
                // 创建新连接，并且创建HttpConnection类管理这个连接
                std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
            }
            catch (const std::exception&) {

            }
        });
}