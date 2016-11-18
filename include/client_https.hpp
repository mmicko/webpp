#ifndef CLIENT_HTTPS_HPP
#define	CLIENT_HTTPS_HPP

#include "client_http.hpp"
#include "asio/ssl.hpp"

namespace webpp {
	using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;
    
    template<>
    class Client<HTTPS> : public ClientBase<HTTPS> {
    public:
	    explicit Client(const std::string& server_port_path, bool verify_certificate=true, 
                const std::string& cert_file=std::string(), const std::string& private_key_file=std::string(), 
                const std::string& verify_file=std::string()) : 
                ClientBase(server_port_path, 443), m_context(asio::ssl::context::tlsv12) {
            if(verify_certificate) {
                m_context.set_verify_mode(asio::ssl::verify_peer);
                m_context.set_default_verify_paths();
            }
            else
                m_context.set_verify_mode(asio::ssl::verify_none);
            
            if(cert_file.size()>0 && private_key_file.size()>0) {
                m_context.use_certificate_chain_file(cert_file);
                m_context.use_private_key_file(private_key_file, asio::ssl::context::pem);
            }
            
            if(verify_file.size()>0)
                m_context.load_verify_file(verify_file);
            
            socket=std::make_shared<HTTPS>(io_context, m_context);
        }

    protected:
        asio::ssl::context m_context;
        
        void connect() override {
            if(socket_error || !socket->lowest_layer().is_open()) {
                asio::ip::tcp::resolver resolver(io_context);
                asio::connect(socket->lowest_layer(), resolver.resolve(host, std::to_string(port)));
                
                asio::ip::tcp::no_delay option(true);
                socket->lowest_layer().set_option(option);
                
                socket->handshake(asio::ssl::stream_base::client);
                
                socket_error=false;
            }
        }
    };
}

#endif	/* CLIENT_HTTPS_HPP */
