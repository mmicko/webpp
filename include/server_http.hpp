#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#if defined(_MSC_VER)
#pragma warning(disable:4503)
#endif

#include "asio_wrapper.hpp"
#include "asio/system_timer.hpp"

#include <unordered_map>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>
#include <regex>

namespace webpp {
    template <class socket_type>
    class ServerBase {
    public:
        virtual ~ServerBase() {}

		class Response {
			friend class ServerBase<socket_type>;

			asio::streambuf m_streambuf;

			std::shared_ptr<socket_type> m_socket;
			std::ostream m_ostream;
			std::stringstream m_header;
			explicit Response(const std::shared_ptr<socket_type> &socket) : m_ostream(&m_streambuf), m_socket(socket) {}

			std::string statusToString(int status)
			{
				switch (status) {
					default:
					case 200: return "HTTP/1.0 200 OK\r\n";
					case 201: return "HTTP/1.0 201 Created\r\n";
					case 202: return "HTTP/1.0 202 Accepted\r\n";
					case 204: return "HTTP/1.0 204 No Content\r\n";
					case 300: return "HTTP/1.0 300 Multiple Choices\r\n";
					case 301: return "HTTP/1.0 301 Moved Permanently\r\n";
					case 302: return "HTTP/1.0 302 Moved Temporarily\r\n";
					case 304: return "HTTP/1.0 304 Not Modified\r\n";
					case 400: return "HTTP/1.0 400 Bad Request\r\n";
					case 401: return "HTTP/1.0 401 Unauthorized\r\n";
					case 403: return "HTTP/1.0 403 Forbidden\r\n";
					case 404: return "HTTP/1.0 404 Not Found\r\n";
					case 500: return "HTTP/1.0 500 Internal Server Error\r\n";
					case 501: return "HTTP/1.0 501 Not Implemented\r\n";
					case 502: return "HTTP/1.0 502 Bad Gateway\r\n";
					case 504: return "HTTP/1.0 503 Service Unavailable\r\n";
				}				
			}
		public:
			Response& status(int number) { m_ostream << statusToString(number); return *this; }
			void type(std::string str) { m_header << "Content-Type: "<< str << "\r\n"; }
			void send(std::string str) { m_ostream << m_header.str() << "Content-Length: " << str.length() << "\r\n\r\n" << str; }
			size_t size() const { return m_streambuf.size(); }
			std::shared_ptr<socket_type> socket() { return m_socket; }
        };
        
        class Content : public std::istream {
            friend class ServerBase<socket_type>;
        public:
            size_t size() const {
                return streambuf.size();
            }
            std::string string() const {
                std::stringstream ss;
                ss << rdbuf();
                return ss.str();
            }
        private:
            asio::streambuf &streambuf;
	        explicit Content(asio::streambuf &streambuf): std::istream(&streambuf), streambuf(streambuf) {}
        };
        
        class Request {
            friend class ServerBase<socket_type>;
            
            class iequal_to {
            public:
              bool operator()(const std::string &key1, const std::string &key2) const {
				   return key1.size() == key2.size()
						&& equal(key1.cbegin(), key1.cend(), key2.cbegin(),
							[](std::string::value_type key1v, std::string::value_type key2v)
								{ return tolower(key1v) == tolower(key2v); });
              }
            };
            class ihash {
            public:
              size_t operator()(const std::string &key) const {
                size_t seed=0;
                for(auto &c: key) {
				  std::hash<char> hasher;
				  seed ^= hasher(std::tolower(c)) + 0x9e3779b9 + (seed<<6) + (seed>>2);
				}
                return seed;
              }
            };
        public:
            std::string method, path, http_version;

            Content content;

            std::unordered_multimap<std::string, std::string, ihash, iequal_to> header;

            std::smatch path_match;
            
            std::string remote_endpoint_address;
            unsigned short remote_endpoint_port;
            
        private:
            Request(): content(streambuf), remote_endpoint_port(0) { }
            
            asio::streambuf streambuf;
        };
        
        class Config {
            friend class ServerBase<socket_type>;

            Config(unsigned short port): port(port), reuse_address(true) {}
        public:
            unsigned short port;
            ///IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
            ///If empty, the address will be any address.
            std::string address;
            ///Set to false to avoid binding the socket to an address that is already in use.
            bool reuse_address;
        };
        ///Set before calling start().
        Config m_config;
        using http_handler = std::function<void(std::shared_ptr<Response>, std::shared_ptr<Request>)>;
    	
		template<class T> void on_get(std::string regex, T&& func) { m_resource[regex]["GET"] = func; }
		template<class T> void on_get(T&& func) { m_default_resource["GET"] = func; }
		template<class T> void on_post(std::string regex, T&& func) { m_resource[regex]["POST"] = func; }
		template<class T> void on_post(T&& func) { m_default_resource["POST"] = func; }
        
	private:
		std::unordered_map<std::string, std::unordered_map<std::string, http_handler>>  m_resource;
        
        std::unordered_map<std::string, http_handler> m_default_resource;
        
        std::function<void(const std::exception&)> m_exception_handler;

        std::vector<std::pair<std::string, std::vector<std::pair<std::regex,http_handler>>>> m_opt_resource;
        
    public:
        void start() {
            //Copy the resources to opt_resource for more efficient request processing
            m_opt_resource.clear();
            for(auto& res: m_resource) {
                for(auto& res_method: res.second) {
                    auto it=m_opt_resource.end();
                    for(auto opt_it=m_opt_resource.begin();opt_it!=m_opt_resource.end();++opt_it) {
                        if(res_method.first==opt_it->first) {
                            it=opt_it;
                            break;
                        }
                    }
                    if(it==m_opt_resource.end()) {
                        m_opt_resource.emplace_back();
                        it=m_opt_resource.begin()+(m_opt_resource.size()-1);
                        it->first=res_method.first;
                    }
                    it->second.emplace_back(std::regex(res.first), res_method.second);
                }
            }

            if(!io_context)
                io_context=std::make_shared<asio::io_context>();

            if(io_context->stopped())
                io_context.reset();

            asio::ip::tcp::endpoint endpoint;
            if(m_config.address.size()>0)
                endpoint=asio::ip::tcp::endpoint(asio::ip::make_address(m_config.address), m_config.port);
            else
                endpoint=asio::ip::tcp::endpoint(asio::ip::tcp::v4(), m_config.port);
            
            if(!acceptor)
                acceptor= std::make_unique<asio::ip::tcp::acceptor>(*io_context);
            acceptor->open(endpoint.protocol());
            acceptor->set_option(asio::socket_base::reuse_address(m_config.reuse_address));
            acceptor->bind(endpoint);
            acceptor->listen();
     
            accept(); 
            
            io_context->run();
        }
        
        void stop() {
            acceptor->close();
            io_context->stop();
        }
        
        ///Use this function if you need to recursively send parts of a longer message
        void send(const std::shared_ptr<Response> &response, const std::function<void(const std::error_code&)>& callback=nullptr) const {
            asio::async_write(*response->socket(), response->m_streambuf, [this, response, callback](const std::error_code& ec, size_t /*bytes_transferred*/) {
                if(callback)
                    callback(ec);
            });
        }

        /// If you have your own asio::io_context, store its pointer here before running start().
        /// You might also want to set config.num_threads to 0.
        std::shared_ptr<asio::io_context> io_context;
    protected:
        std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
        std::vector<std::thread> threads;
        
        long timeout_request;
        long timeout_content;
        
        ServerBase(unsigned short port, long timeout_request, long timeout_send_or_receive) :
                m_config(port), timeout_request(timeout_request), timeout_content(timeout_send_or_receive) {}
        
        virtual void accept()=0;
        
        std::shared_ptr<asio::system_timer> set_timeout_on_socket(const std::shared_ptr<socket_type> &socket, long seconds) {
            auto timer = std::make_shared<asio::system_timer>(*io_context);
            timer->expires_at(std::chrono::system_clock::now() + std::chrono::seconds(seconds));
            timer->async_wait([socket](const std::error_code& ec){
                if(!ec) {
					std::error_code newec = ec;
                    socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, newec);
                    socket->lowest_layer().close();
                }
            });
            return timer;
        }
        
        void read_request_and_content(const std::shared_ptr<socket_type> &socket) {
            //Create new streambuf (Request::streambuf) for async_read_until()
            //shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<Request> request(new Request());
            try {
                request->remote_endpoint_address = socket->lowest_layer().remote_endpoint().address().to_string();
                request->remote_endpoint_port = socket->lowest_layer().remote_endpoint().port();
            }
            catch(const std::exception &e) {
                if(m_exception_handler)
                   m_exception_handler(e);
            }

            //Set timeout on the following asio::async-read or write function
            std::shared_ptr<asio::system_timer> timer;
            if(timeout_request>0)
                timer=set_timeout_on_socket(socket, timeout_request);
                        
            asio::async_read_until(*socket, request->streambuf, "\r\n\r\n",
                    [this, socket, request, timer](const std::error_code& ec, size_t bytes_transferred) {
                if(timeout_request>0)
                    timer->cancel();
                if(!ec) {
                    //request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                    //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                    //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                    //streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                    size_t num_additional_bytes=request->streambuf.size()-bytes_transferred;
                    
                    if(!parse_request(request, request->content))
                        return;
                    
                    //If content, read that as well
                    auto it=request->header.find("Content-Length");
                    if(it!=request->header.end()) {
                        //Set timeout on the following asio::async-read or write function
                        std::shared_ptr<asio::system_timer> timer2;
                        if(timeout_content>0)
                            timer2=set_timeout_on_socket(socket, timeout_content);
                        unsigned long long content_length;
                        try {
                            content_length=stoull(it->second);
                        }
                        catch(const std::exception &e) {
                            if(m_exception_handler)
                                m_exception_handler(e);
                            return;
                        }
                        if(content_length>num_additional_bytes) {
                            asio::async_read(*socket, request->streambuf,
                                    asio::transfer_exactly(size_t(content_length)-num_additional_bytes),
                                    [this, socket, request, timer2]
                                    (const std::error_code& ec, size_t /*bytes_transferred*/) {
                                if(timeout_content>0)
                                    timer2->cancel();
                                if(!ec)
                                    find_resource(socket, request);
                            });
                        }
                        else {
                            if(timeout_content>0)
                                timer2->cancel();
                            find_resource(socket, request);
                        }
                    }
                    else {
                        find_resource(socket, request);
                    }
                }
            });
        }

        bool parse_request(const std::shared_ptr<Request> &request, std::istream& stream) const {
            std::string line;
            getline(stream, line);
            size_t method_end;
            if((method_end=line.find(' '))!=std::string::npos) {
                size_t path_end;
                if((path_end=line.find(' ', method_end+1))!=std::string::npos) {
                    request->method=line.substr(0, method_end);
                    request->path=line.substr(method_end+1, path_end-method_end-1);

                    size_t protocol_end;
                    if((protocol_end=line.find('/', path_end+1))!=std::string::npos) {
                        if(line.substr(path_end+1, protocol_end-path_end-1)!="HTTP")
                            return false;
                        request->http_version=line.substr(protocol_end+1, line.size()-protocol_end-2);
                    }
                    else
                        return false;

                    getline(stream, line);
                    size_t param_end;
                    while((param_end=line.find(':'))!=std::string::npos) {
                        size_t value_start=param_end+1;
                        if((value_start)<line.size()) {
                            if(line[value_start]==' ')
                                value_start++;
                            if(value_start<line.size())
                                request->header.insert(make_pair(line.substr(0, param_end), line.substr(value_start, line.size()-value_start-1)));
                        }
    
                        getline(stream, line);
                    }
                }
                else
                    return false;
            }
            else
                return false;
            return true;
        }

        void find_resource(const std::shared_ptr<socket_type> &socket, const std::shared_ptr<Request> &request) {
            //Find path- and method-match, and call write_response
            for(auto& res: m_opt_resource) {
                if(request->method==res.first) {
                    for(auto& res_path: res.second) {
                        std::smatch sm_res;
                        if(std::regex_match(request->path, sm_res, res_path.first)) {
                            request->path_match=move(sm_res);
                            write_response(socket, request, res_path.second);
                            return;
                        }
                    }
                }
            }
            auto it_method=m_default_resource.find(request->method);
            if(it_method!=m_default_resource.end()) {
                write_response(socket, request, it_method->second);
            }
        }
        bool iequals(const std::string &key1, const std::string &key2) const {
			return key1.size() == key2.size()
				&& equal(key1.cbegin(), key1.cend(), key2.cbegin(),
						[](std::string::value_type key1v, std::string::value_type key2v)
							{ return tolower(key1v) == tolower(key2v); });
		}
							
        void write_response(const std::shared_ptr<socket_type> &socket, const std::shared_ptr<Request> &request, http_handler& resource_function) {
            //Set timeout on the following asio::async-read or write function
            std::shared_ptr<asio::system_timer> timer;
            if(timeout_content>0)
                timer=set_timeout_on_socket(socket, timeout_content);

            auto response=std::shared_ptr<Response>(new Response(socket), [this, request, timer](Response *response_ptr) {
                auto response=std::shared_ptr<Response>(response_ptr);
                send(response, [this, response, request, timer](const std::error_code& ec) {
                    if(!ec) {
                        if(timeout_content>0)
                            timer->cancel();
                        float http_version;
                        try {
                            http_version=stof(request->http_version);
                        }
                        catch(const std::exception &e){
                            if(m_exception_handler)
                                m_exception_handler(e);
                            return;
                        }
                        
                        auto range=request->header.equal_range("Connection");
                        for(auto it=range.first;it!=range.second; ++it) {
                            if(iequals(it->second, "close"))
                                return;
                        }
                        if(http_version>1.05)
                            read_request_and_content(response->socket());
                    }
                });
            });

            try {
                resource_function(response, request);
            }
            catch(const std::exception &e) {
                if(m_exception_handler)
                    m_exception_handler(e);
            }
        }
    };
    
    template<class socket_type>
    class Server : public ServerBase<socket_type> {
    public:
	    Server(unsigned short port, size_t num_threads, long timeout_request, long timeout_send_or_receive)
		    : ServerBase<socket_type>(port, num_threads, timeout_request, timeout_send_or_receive)
	    {
	    }
    };

	using HTTP = asio::ip::tcp::socket;
    
    template<>
    class Server<HTTP> : public ServerBase<HTTP> {
    public:
	    explicit Server(unsigned short port, long timeout_request=5, long timeout_content=300) :
                ServerBase(port, timeout_request, timeout_content) {}
        
    protected:
        void accept() override {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            auto socket = std::make_shared<HTTP>(*io_context);
                        
            acceptor->async_accept(*socket, [this, socket](const std::error_code& ec){
                //Immediately start accepting a new connection (if io_context hasn't been stopped)
                if (ec != asio::error::operation_aborted)
                    accept();
                                
                if(!ec) {
                    asio::ip::tcp::no_delay option(true);
                    socket->set_option(option);
                    
                    read_request_and_content(socket);
                }
            });
        }
    };
}
#endif	/* SERVER_HTTP_HPP */
