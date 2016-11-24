#ifndef CLIENT_HTTP_HPP
#define	CLIENT_HTTP_HPP

#if defined(_MSC_VER)
#pragma warning(disable:4503)
#endif

#include "asio_wrapper.hpp"

#include <unordered_map>
#include <map>
#include <random>
#include <mutex>

namespace webpp {
    template <class socket_type>
    class ClientBase {
    public:
        virtual ~ClientBase() {}

        class Response {
            friend class ClientBase<socket_type>;
            
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
            std::string http_version, status_code;

            std::istream content;

            std::unordered_multimap<std::string, std::string, ihash, iequal_to> header;
            
        private:
            asio::streambuf content_buffer;
            
            Response(): content(&content_buffer) {}
        };
        
		class Config {
			friend class ClientBase<socket_type>;
		private:
			Config() {}
		public:
			/// Set timeout on requests in seconds. Default value: 0 (no timeout). 
			size_t timeout = 0;
			/// Set proxy server (server:port)
			std::string proxy_server;
		};

		/// Set before calling request
		Config config;

        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path="/", const std::string content="",
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            auto corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
			if (!config.proxy_server.empty())
				corrected_path = protocol() + "://" + config.proxy_server + corrected_path;

            asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content.size()>0)
                write_stream << "Content-Length: " << content.size() << "\r\n";
            write_stream << "\r\n";
           			
			connect();

			auto timer = get_timeout_timer();
			asio::async_write(*socket, write_buffer,
				[this, &content, timer](const std::error_code &ec, size_t /*bytes_transferred*/)
			{
				if (timer)
					timer->cancel();
				if (!ec) {
					if (!content.empty()) {
						auto timer = get_timeout_timer();
						asio::async_write(*socket, asio::buffer(content.data(), content.size()),
							[this,timer](const std::error_code &ec, size_t /*bytes_transferred*/) {
							if (timer)
								timer->cancel();
							if (ec) {
								std::lock_guard<std::mutex> lock(socket_mutex);
								socket = nullptr;
								throw std::system_error(ec);
							}
						});
					}
				}
				else {
					std::lock_guard<std::mutex> lock(socket_mutex);
					socket = nullptr;
					throw std::system_error(ec);
				}
			});
			io_context.reset();
			io_context.run();
            return request_read();
        }
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path, std::iostream& content,
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            auto corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
			if (!config.proxy_server.empty())
				corrected_path = protocol() + "://" + config.proxy_server + corrected_path;
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            
            asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content_length>0)
                write_stream << "Content-Length: " << content_length << "\r\n";
            write_stream << "\r\n";
            if(content_length>0)
                write_stream << content.rdbuf();
			
			connect();

			auto timer = get_timeout_timer();
			asio::async_write(*socket, write_buffer,
				[this,timer](const std::error_code &ec, size_t /*bytes_transferred*/) {
				if (timer)
					timer->cancel();
				if (ec) {
					std::lock_guard<std::mutex> lock(socket_mutex);
					socket = nullptr;
					throw std::system_error(ec);
				}
			});
			io_service.reset();
			io_service.run();
            
            return request_read();
        }
		void close() {
			std::lock_guard<std::mutex> lock(socket_mutex);
			if (socket) {
				std::error_code ec;
				socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
				socket->lowest_layer().close();
			}
		}
    protected:
        asio::io_context io_context;
        asio::ip::tcp::resolver resolver;
        
        std::unique_ptr<socket_type> socket;
		std::mutex socket_mutex;

        std::string host;
        unsigned short port;
                
		ClientBase(const std::string& host_port, unsigned short default_port) : resolver(io_context) {
			auto parsed_host_port = parse_host_port(host_port, default_port);
			host = parsed_host_port.first;
			port = parsed_host_port.second;
		}

		std::pair<std::string, unsigned short> parse_host_port(const std::string &host_port, unsigned short default_port) {
			std::pair<std::string, unsigned short> parsed_host_port;
            size_t host_end=host_port.find(':');
            if(host_end==std::string::npos) {
				parsed_host_port.first =host_port;
				parsed_host_port.second =default_port;
            }
            else {
				parsed_host_port.first =host_port.substr(0, host_end);
				parsed_host_port.second =static_cast<unsigned short>(stoul(host_port.substr(host_end+1)));
            }
			return parsed_host_port;
        }
        
		virtual std::string protocol() = 0;
        virtual void connect()=0;
        
		std::shared_ptr<asio::system_timer> get_timeout_timer() {
			if (config.timeout == 0)
				return nullptr;

			auto timer = std::make_shared<asio::system_timer>(io_context);
			timer->expires_from_now(std::chrono::seconds(config.timeout));
			timer->async_wait([this](const std::error_code& ec) {
				if (!ec) {
					close();
				}
			});
			return timer;
		}

        void parse_response_header(const std::shared_ptr<Response> &response) const {
            std::string line;
            getline(response->content, line);
            size_t version_end=line.find(' ');
            if(version_end!=std::string::npos) {
                if(5<line.size())
                    response->http_version=line.substr(5, version_end-5);
                if((version_end+1)<line.size())
                    response->status_code=line.substr(version_end+1, line.size()-(version_end+1)-1);

                getline(response->content, line);
                size_t param_end;
                while((param_end=line.find(':'))!=std::string::npos) {
                    size_t value_start=param_end+1;
                    if((value_start)<line.size()) {
                        if(line[value_start]==' ')
                            value_start++;
                        if(value_start<line.size())
                            response->header.insert(std::make_pair(line.substr(0, param_end), line.substr(value_start, line.size()-value_start-1)));
                    }

                    getline(response->content, line);
                }
            }
        }
        
		std::shared_ptr<Response> request_read() {
			std::shared_ptr<Response> response(new Response());

			asio::streambuf chunked_streambuf;

			auto timer = get_timeout_timer();
			asio::async_read_until(*socket, response->content_buffer, "\r\n\r\n",
				[this, &response, &chunked_streambuf,timer](const std::error_code& ec, size_t bytes_transferred) {
				if (timer)
					timer->cancel();
				if (!ec) {
					size_t num_additional_bytes = response->content_buffer.size() - bytes_transferred;

					parse_response_header(response);

					auto header_it = response->header.find("Content-Length");
					if (header_it != response->header.end()) {
						auto content_length = stoull(header_it->second);
						if (content_length>num_additional_bytes) {
							auto timer = get_timeout_timer();
							asio::async_read(*socket, response->content_buffer,
								asio::transfer_exactly(size_t(content_length) - num_additional_bytes),
								[this,timer](const std::error_code& ec, size_t /*bytes_transferred*/) {
								if (timer)
									timer->cancel();
								if (ec) {
									std::lock_guard<std::mutex> lock(socket_mutex);
									socket = nullptr;
									throw std::system_error(ec);
								}
							});
						}
					}
					else if ((header_it = response->header.find("Transfer-Encoding")) != response->header.end() && header_it->second == "chunked") {
						request_read_chunked(response, chunked_streambuf);
					}
				}
				else {
					std::lock_guard<std::mutex> lock(socket_mutex);
					socket = nullptr;
					throw std::system_error(ec);
				}
			});
			io_context.reset();
			io_context.run();

			return response;
		}

		void request_read_chunked(const std::shared_ptr<Response> &response, asio::streambuf &streambuf) {
			auto timer = get_timeout_timer();
			asio::async_read_until(*socket, response->content_buffer, "\r\n",
				[this, &response, &streambuf,timer](const std::error_code& ec, size_t bytes_transferred) {
				if (timer)
					timer->cancel();
				if (!ec) {
					std::string line;
					getline(response->content, line);
					bytes_transferred -= line.size() + 1;
					line.pop_back();
					std::streamsize length = stol(line, 0, 16);

					auto num_additional_bytes = static_cast<std::streamsize>(response->content_buffer.size() - bytes_transferred);

					auto post_process = [this, &response, &streambuf, length] {
						std::ostream stream(&streambuf);
						if (length > 0) {
							std::vector<char> buffer(static_cast<size_t>(length));
							response->content.read(&buffer[0], length);
							stream.write(&buffer[0], length);
						}

						//Remove "\r\n"
						response->content.get();
						response->content.get();

						if (length>0)
							request_read_chunked(response, streambuf);
						else {
							std::ostream response_stream(&response->content_buffer);
							response_stream << stream.rdbuf();
						}
					};

					if ((2 + length)>num_additional_bytes) {
						auto timer = get_timeout_timer();
						asio::async_read(*socket, response->content_buffer,
							asio::transfer_exactly(size_t(2 + length) - size_t(num_additional_bytes)),
							[this, post_process, timer](const std::error_code& ec, size_t /*bytes_transferred*/) {
							if (timer)
								timer->cancel();
							if (!ec) {
								post_process();
							}
							else {
								std::lock_guard<std::mutex> lock(socket_mutex);
								socket = nullptr;
								throw std::system_error(ec);
							}
						});
					}
					else
						post_process();
				}
				else {
					std::lock_guard<std::mutex> lock(socket_mutex);
					socket = nullptr;
					throw std::system_error(ec);
				}
			});
		}
	};
	
    template<class socket_type>
    class Client : public ClientBase<socket_type> {
    public:
	    Client(const std::string& host_port, unsigned short default_port)
		    : ClientBase<socket_type>(host_port, default_port)
	    {
	    }
    };

	using HTTP = asio::ip::tcp::socket;
    
    template<>
    class Client<HTTP> : public ClientBase<HTTP> {
    public:
		explicit Client(const std::string& server_port_path) : ClientBase(server_port_path, 80) { }
        
    protected:
		std::string protocol() {
			return "http";
		}

        void connect() override {
            if(!socket || !socket->is_open()) {
                asio::ip::tcp::resolver resolver(io_context);
				std::string host, port;
				if (config.proxy_server.empty()) {
					host = this->host;
					port = std::to_string(this->port);					
				}
				else {
					auto proxy_host_port = parse_host_port(config.proxy_server, 0);
					host = proxy_host_port.first;
					port = std::to_string(proxy_host_port.second);					
				}
				asio::ip::tcp::resolver::query query(host, port);
                resolver.async_resolve(query, [this](const std::error_code &ec,
                                                      asio::ip::tcp::resolver::iterator it){
                    if(!ec) {
						{
							std::lock_guard<std::mutex> lock(socket_mutex);
							socket = std::unique_ptr<HTTP>(new HTTP(io_context));
						}
                        asio::async_connect(*socket, it, [this]
                                (const std::error_code &ec, asio::ip::tcp::resolver::iterator /*it*/){
                            if(!ec) {
                                asio::ip::tcp::no_delay option(true);
                                socket->set_option(option);
                            }
                            else {
								std::lock_guard<std::mutex> lock(socket_mutex);
                                socket=nullptr;
                                throw std::system_error(ec);
                            }
                        });
                    }
                    else {
						std::lock_guard<std::mutex> lock(socket_mutex);
                        socket=nullptr;
                        throw std::system_error(ec);
                    }
                });
				io_context.reset();
				io_context.run();       
			}
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */
