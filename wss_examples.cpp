// license:MIT
// copyright-holders:Ole Christian Eidheim, Miodrag Milanovic
#include "server_wss.hpp"
#include "client_wss.hpp"

int main() {
	//WebSocket Secure (WSS)-server at port 8080 using 1 thread
	webpp::wss_server server("server.crt", "server.key");
	server.config.port = 8080;

	//Example 1: echo WebSocket Secure endpoint
	//  Added debug messages for example use of the callbacks
	//  Test with the following JavaScript:
	//    var wss=new WebSocket("wss://localhost:8080/echo");
	//    wss.onmessage=function(evt){console.log(evt.data);};
	//    wss.send("test");
	auto& echo=server.endpoint["/echo"];

	echo.on_message=[&server](auto connection, auto message) {
		//WssServer::Message::string() is a convenience function for:
		//stringstream data_ss;
		//data_ss << message->rdbuf();
		//auto message_str = data_ss.str();
		auto message_str=message->string();

		std::cout << "Server: Message received: \"" << message_str << "\" from " << size_t(connection.get()) << std::endl;

		std::cout << "Server: Sending message \"" << message_str <<  "\" to " << size_t(connection.get()) << std::endl;

		auto send_stream = std::make_shared<webpp::wss_server::SendStream>();
		*send_stream << message_str;
		//server.send is an asynchronous function
		server.send(connection, send_stream, [](const std::error_code& ec){
			if(ec) {
				std::cout << "Server: Error sending message. " <<
				//See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
						"Error: " << ec << ", error message: " << ec.message() << std::endl;
			}
		});
	};

	echo.on_open=[](auto connection) {
		std::cout << "Server: Opened connection " << size_t(connection.get()) << std::endl;
	};

	//See RFC 6455 7.4.1. for status codes
	echo.on_close=[](auto connection, int status, const std::string& /*reason*/) {
		std::cout << "Server: Closed connection " << size_t(connection.get()) << " with status code " << status << std::endl;
	};

	//See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
	echo.on_error=[](auto connection, const std::error_code& ec) {
		std::cout << "Server: Error in connection " << size_t(connection.get()) << ". " <<
				"Error: " << ec << ", error message: " << ec.message() << std::endl;
	};

	//Example 2: Echo thrice
	//  Send a received message three times back to the client
	//  Test with the following JavaScript:
	//    var wss=new WebSocket("wss://localhost:8080/echo_thrice");
	//    wss.onmessage=function(evt){console.log(evt.data);};
	//    wss.send("test");
	auto& echo_thrice=server.endpoint["/echo_thrice"];
	echo_thrice.on_message=[&server](auto connection, auto message) {
		auto message_str=message->string();

		auto send_stream1 = std::make_shared<webpp::wss_server::SendStream>();
		*send_stream1 << message_str;
		//server.send is an asynchronous function
		server.send(connection, send_stream1, [&server, connection, message_str](const std::error_code& ec) {
			if(!ec) {
				auto send_stream3 = std::make_shared<webpp::wss_server::SendStream>();
				*send_stream3 << message_str;
				server.send(connection, send_stream3); //Sent after send_stream1 is sent, and most likely after send_stream2
			}
		});
		//Do not reuse send_stream1 here as it most likely is not sent yet
		auto send_stream2 = std::make_shared<webpp::wss_server::SendStream>();
		*send_stream2 << message_str;
		server.send(connection, send_stream2); //Most likely queued, and sent after send_stream1
	};

	//Example 3: Echo to all WebSocket Secure endpoints
	//  Sending received messages to all connected clients
	//  Test with the following JavaScript on more than one browser windows:
	//    var wss=new WebSocket("wss://localhost:8080/echo_all");
	//    wss.onmessage=function(evt){console.log(evt.data);};
	//    wss.send("test");
	auto& echo_all=server.endpoint["/echo_all"];
	echo_all.on_message=[&server](auto /*connection*/, auto message) {
		auto message_str=message->string();

		//echo_all.get_connections() can also be used to solely receive connections on this endpoint
		for(auto a_connection: server.get_connections()) {
			auto send_stream = std::make_shared<webpp::wss_server::SendStream>();
			*send_stream << message_str;

			//server.send is an asynchronous function
			server.send(a_connection, send_stream);
		}
	};

	std::thread server_thread([&server](){
		//Start WSS-server
		server.start();
	});

	//Wait for server to start so that the client can connect
	std::this_thread::sleep_for(std::chrono::seconds(1));

	//Example 4: Client communication with server
	//Second Client() parameter set to false: no certificate verification
	//Possible output:
	//Server: Opened connection 140184920260656
	//Client: Opened connection
	//Client: Sending message: "Hello"
	//Server: Message received: "Hello" from 140184920260656
	//Server: Sending message "Hello" to 140184920260656
	//Client: Message received: "Hello"
	//Client: Sending close connection
	//Server: Closed connection 140184920260656 with status code 1000
	//Client: Closed connection with status code 1000
	webpp::wss_client client("localhost:8080/echo", false);
	client.on_message=[&client](auto message) {
		auto message_str=message->string();

		std::cout << "Client: Message received: \"" << message_str << "\"" << std::endl;

		std::cout << "Client: Sending close connection" << std::endl;
		client.send_close(1000);
	};

	client.on_open=[&client]() {
		std::cout << "Client: Opened connection" << std::endl;

		std::string message="Hello";
		std::cout << "Client: Sending message: \"" << message << "\"" << std::endl;

		auto send_stream = std::make_shared<webpp::wss_client::SendStream>();
		*send_stream << message;
		client.send(send_stream);
	};

	client.on_close=[](int status, const std::string& /*reason*/) {
		std::cout << "Client: Closed connection with status code " << status << std::endl;
	};

	//See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
	client.on_error=[](const std::error_code& ec) {
		std::cout << "Client: Error: " << ec << ", error message: " << ec.message() << std::endl;
	};

	client.start();

	server_thread.join();

	return 0;
}
