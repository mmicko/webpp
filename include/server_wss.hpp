#ifndef SERVER_WSS_HPP
#define	SERVER_WSS_HPP

#include "server_ws.hpp"
#include "asio/ssl.hpp"

namespace webpp {
	using WSS = asio::ssl::stream<asio::ip::tcp::socket>;    
        
    template<>
    class SocketServer<WSS> : public SocketServerBase<WSS> {
        
    public:
        SocketServer(unsigned short port, size_t num_threads, const std::string& cert_file, const std::string& private_key_file, 
                size_t timeout_request=5, size_t timeout_idle=0, 
                const std::string& verify_file=std::string()) : 
                SocketServerBase(port, num_threads, timeout_request, timeout_idle), 
                context(asio::ssl::context::tlsv12) {
            context.use_certificate_chain_file(cert_file);
            context.use_private_key_file(private_key_file, asio::ssl::context::pem);
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
        }

    protected:
        asio::ssl::context context;
        
        void accept() override {
            //Create new socket for this connection (stored in Connection::socket)
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<Connection> connection(new Connection(new WSS(*io_context, context)));
            
            acceptor->async_accept(connection->socket->lowest_layer(), [this, connection](const std::error_code& ec) {
                //Immediately start accepting a new connection (if io_context hasn't been stopped)
                if (ec != asio::error::operation_aborted)
                    accept();

                if(!ec) {
                    asio::ip::tcp::no_delay option(true);
                    connection->socket->lowest_layer().set_option(option);
                    
                    //Set timeout on the following asio::ssl::stream::async_handshake
                    std::shared_ptr<asio::system_timer> timer;
                    if(timeout_request>0)
                        timer=set_timeout_on_connection(connection, timeout_request);
                    connection->socket->async_handshake(asio::ssl::stream_base::server, 
                            [this, connection, timer](const std::error_code& ec) {
                        if(timeout_request>0)
                            timer->cancel();
                        if(!ec)
                            read_handshake(connection);
                    });
                }
            });
        }
    };
}


#endif	/* SERVER_WSS_HPP */

