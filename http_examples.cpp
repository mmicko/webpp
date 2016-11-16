#include "server_http.hpp"
#include "client_http.hpp"

#include <fstream>
#include <vector>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

//Added for the default_resource example
void default_resource_send(const HttpServer &server, const std::shared_ptr<HttpServer::Response> &response,
                           const std::shared_ptr<std::ifstream> &ifs);

int main() {
    //HTTP-server at port 8080 using 1 thread
    //Unless you do more heavy non-threaded processing in the resources,
    //1 thread is usually faster than several threads
    HttpServer server(8080, 1);
    
    //Add resources using path-regex and method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    server.on_post("^/string$", [](auto response, auto request) {
        //Retrieve string:
        auto content=request->content.string();
        //request->content.string() is a convenience function for:
        //stringstream ss;
        //ss << request->content.rdbuf();
        //string content=ss.str();
        
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    });
    
    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    server.on_post("^/json$", [](auto response, auto /*request*/) {
        try {

			std::string name="Test Name ";

            *response << "HTTP/1.1 200 OK\r\n"
                      << "Content-Type: application/json\r\n"
                      << "Content-Length: " << name.length() << "\r\n\r\n"
                      << name;
        }
        catch(std::exception& e) {
            *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    });

    //GET-example for the path /info
    //Responds with request-information
    server.on_get("^/info$", [](auto response, auto request) {
		std::stringstream content_stream;
        content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
        content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
        for(auto& header: request->header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        //find length of content_stream (length received using content_stream.tellp())
        content_stream.seekp(0, std::ios::end);
        
        *response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    });
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    server.on_get("^/match/([0-9]+)$", [&server](auto response, auto request) {
		std::string number=request->path_match[1];
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    });
    
    //Get example simulating heavy work in a separate thread
    server.on_get("^/work$", [&server](auto response, auto /*request*/) {
		std::thread work_thread([response] {
			std::this_thread::sleep_for(std::chrono::seconds(5));
			std::string message="Work done";
            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << message.length() << "\r\n\r\n" << message;
        });
        work_thread.detach();
    });
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
	server.on_get([&server](auto response, auto request) {
		std::string message = "Dummy";
		*response << "HTTP/1.1 200 OK\r\nContent-Length: " << message.length() << "\r\n\r\n" << message;
//        try {
//            auto web_root_path=boost::filesystem::canonical("web");
//            auto path=boost::filesystem::canonical(web_root_path/request->path);
//            //Check if path is within web_root_path
//            if(distance(web_root_path.begin(), web_root_path.end())>distance(path.begin(), path.end()) ||
//               !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
//                throw invalid_argument("path must be within root path");
//            if(boost::filesystem::is_directory(path))
//                path/="index.html";
//            if(!(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)))
//                throw invalid_argument("file does not exist");
//            
//            auto ifs=make_shared<ifstream>();
//            ifs->open(path.string(), ifstream::in | ios::binary);
//            
//            if(*ifs) {
//                ifs->seekg(0, ios::end);
//                auto length=ifs->tellg();
//                
//                ifs->seekg(0, ios::beg);
//                
//                *response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
//                default_resource_send(server, response, ifs);
//            }
//            else
//                throw invalid_argument("could not read file");
//        }
//        catch(const exception &e) {
//            string content="Could not open path "+request->path+": "+e.what();
//            *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
//        }
    });
    
	std::thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    //Wait for server to start so that the client can connect
	std::this_thread::sleep_for(std::chrono::seconds(1));
    
    //Client examples
    HttpClient client("localhost:8080");
    auto r1=client.request("GET", "/match/123");
	std::cout << r1->content.rdbuf() << std::endl;

	std::string json_string="{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
    auto r2=client.request("POST", "/string", json_string);
	std::cout << r2->content.rdbuf() << std::endl;
    
    auto r3=client.request("POST", "/json", json_string);
	std::cout << r3->content.rdbuf() << std::endl;
        
    server_thread.join();
    
    return 0;
}

void default_resource_send(const HttpServer &server, const std::shared_ptr<HttpServer::Response> &response,
                           const std::shared_ptr<std::ifstream> &ifs) {
    //read and send 128 KB at a time
    static std::vector<char> buffer(131072); // Safe when server is running on one thread
	std::streamsize read_length;
    if((read_length=ifs->read(&buffer[0], buffer.size()).gcount())>0) {
        response->write(&buffer[0], read_length);
        if(read_length==static_cast<std::streamsize>(buffer.size())) {
            server.send(response, [&server, response, ifs](const std::error_code &ec) {
                if(!ec)
                    default_resource_send(server, response, ifs);
                else
					std::cerr << "Connection interrupted" << std::endl;
            });
        }
    }
}
