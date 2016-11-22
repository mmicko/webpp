#ifndef SERVER_HTTPS_HPP
#define	SERVER_HTTPS_HPP

#include "server_http.hpp"
#include "asio/ssl.hpp"

namespace webpp {
	using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;    
    
    template<>
    class Server<HTTPS> : public ServerBase<HTTPS> {
    public:
        Server(unsigned short port, const std::string& cert_file, const std::string& private_key_file,
                long timeout_request=5, long timeout_content=300,
                const std::string& verify_file=std::string()) : 
                ServerBase(port, timeout_request, timeout_content), 
                context(asio::ssl::context::tlsv12) { // 2016/08/13 only use tls12, see https://www.ssllabs.com/ssltest
            context.use_certificate_chain_file(cert_file);
            context.use_private_key_file(private_key_file, asio::ssl::context::pem);
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
        }

    protected:
        asio::ssl::context context;
        
        void accept() override {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
	        auto socket = std::make_shared<HTTPS>(*io_context, context);

            acceptor->async_accept((*socket).lowest_layer(), [this, socket](const std::error_code& ec) {
                //Immediately start accepting a new connection (if io_context hasn't been stopped)
                if (ec != asio::error::operation_aborted)
                    accept();

                
                if(!ec) {
                    asio::ip::tcp::no_delay option(true);
                    socket->lowest_layer().set_option(option);
                    
                    //Set timeout on the following asio::ssl::stream::async_handshake
                    std::shared_ptr<asio::system_timer> timer;
                    if(timeout_request>0)
                        timer=set_timeout_on_socket(socket, timeout_request);
                    (*socket).async_handshake(asio::ssl::stream_base::server, [this, socket, timer]
                            (const std::error_code& ec) {
                        if(timeout_request>0)
                            timer->cancel();
                        if(!ec)
                            read_request_and_content(socket);
                    });
                }
            });
        }
    };
}


#endif	/* SERVER_HTTPS_HPP */

