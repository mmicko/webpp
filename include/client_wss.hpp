#ifndef CLIENT_WSS_HPP
#define	CLIENT_WSS_HPP

#include "client_ws.hpp"
#include "asio/ssl.hpp"

namespace SimpleWeb {
    typedef asio::ssl::stream<asio::ip::tcp::socket> WSS;    
    
    template<>
    class SocketClient<WSS> : public SocketClientBase<WSS> {
    public:
        SocketClient(const std::string& server_port_path, bool verify_certificate=true, 
                const std::string& cert_file=std::string(), const std::string& private_key_file=std::string(), 
                const std::string& verify_file=std::string()) : 
                SocketClientBase<WSS>::SocketClientBase(server_port_path, 443),
                context(asio::ssl::context::tlsv12) {
            if(verify_certificate) {
                context.set_verify_mode(asio::ssl::verify_peer);
                context.set_default_verify_paths();
            }
            else
                context.set_verify_mode(asio::ssl::verify_none);
            
            if(cert_file.size()>0 && private_key_file.size()>0) {
                context.use_certificate_chain_file(cert_file);
                context.use_private_key_file(private_key_file, asio::ssl::context::pem);
            }
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);

        };

    protected:
        asio::ssl::context context;
        
        void connect() {
            asio::ip::tcp::resolver::query query(host, std::to_string(port));
            
            resolver->async_resolve(query, [this]
                    (const std::error_code &ec, asio::ip::tcp::resolver::iterator it){
                if(!ec) {
                    connection=std::shared_ptr<Connection>(new Connection(new WSS(*io_service, context)));
                    
                    asio::async_connect(connection->socket->lowest_layer(), it, [this]
                            (const std::error_code &ec, asio::ip::tcp::resolver::iterator /*it*/){
                        if(!ec) {
                            asio::ip::tcp::no_delay option(true);
                            connection->socket->lowest_layer().set_option(option);
                            
                            connection->socket->async_handshake(asio::ssl::stream_base::client, 
                                    [this](const std::error_code& ec) {
                                if(!ec)
                                    handshake();
                                else
                                    throw std::invalid_argument(ec.message());
                            });
                        }
                        else
                            throw std::invalid_argument(ec.message());
                    });
                }
                else
                    throw std::invalid_argument(ec.message());
            });
        }
    };
}

#endif	/* CLIENT_WSS_HPP */