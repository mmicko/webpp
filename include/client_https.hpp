#ifndef CLIENT_HTTPS_HPP
#define	CLIENT_HTTPS_HPP

#include "client_http.hpp"
#include "asio/ssl.hpp"

namespace SimpleWeb {
    typedef asio::ssl::stream<asio::ip::tcp::socket> HTTPS;
    
    template<>
    class Client<HTTPS> : public ClientBase<HTTPS> {
    public:
        Client(const std::string& server_port_path, bool verify_certificate=true, 
                const std::string& cert_file=std::string(), const std::string& private_key_file=std::string(), 
                const std::string& verify_file=std::string()) : 
                ClientBase<HTTPS>::ClientBase(server_port_path, 443), context(asio::ssl::context::tlsv12) {
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
            
            socket=std::make_shared<HTTPS>(io_service, context);
        }

    protected:
        asio::ssl::context context;
        
        void connect() override {
            if(socket_error || !socket->lowest_layer().is_open()) {
                asio::ip::tcp::resolver::query query(host, std::to_string(port));
                asio::connect(socket->lowest_layer(), resolver.resolve(query));
                
                asio::ip::tcp::no_delay option(true);
                socket->lowest_layer().set_option(option);
                
                socket->handshake(asio::ssl::stream_base::client);
                
                socket_error=false;
            }
        }
    };
}

#endif	/* CLIENT_HTTPS_HPP */
