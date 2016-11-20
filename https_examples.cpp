#include "server_https.hpp"
#include "client_https.hpp"

using HttpsServer = webpp::Server<webpp::HTTPS>;
using HttpsClient = webpp::Client<webpp::HTTPS>;


int main() {
    //HTTPS-server at port 8080 using 1 thread
    //Unless you do more heavy non-threaded processing in the resources,
    //1 thread is usually faster than several threads
    HttpsServer server(8080, 1, "server.crt", "server.key");
    
	//Add resources using path-regex and method-string, and an anonymous function
	//POST-example for the path /string, responds the posted string
	server.on_post("^/string$", [](auto response, auto request) {
		//Retrieve string:
		auto content = request->content.string();
		response->status(200).send(content);
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

			std::string name = "Test Name ";
			response->type("application/json");
			response->status(200).send(name);
		}
		catch (std::exception& e) {
			response->status(400).send(e.what());
		}
	});

	//GET-example for the path /info
	//Responds with request-information
	server.on_get("^/info$", [](auto response, auto request) {
		std::stringstream content_stream;
		content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
		content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
		for (auto& header : request->header) {
			content_stream << header.first << ": " << header.second << "<br>";
		}

		response->status(200).send(content_stream.str());
	});

	//GET-example for the path /match/[number], responds with the matched string in path (number)
	//For instance a request GET /match/123 will receive: 123
	server.on_get("^/match/([0-9]+)$", [&server](auto response, auto request) {
		std::string number = request->path_match[1];
		response->status(200).send(number);
	});

	//Get example simulating heavy work in a separate thread
	server.on_get("^/work$", [&server](auto response, auto /*request*/) {
		std::thread work_thread([response] {
			std::this_thread::sleep_for(std::chrono::seconds(5));
			std::string message = "Work done";
			response->status(200).send(message);
		});
		work_thread.detach();
	});

	//Default GET-example. If no other matches, this anonymous function will be called. 
	//Will respond with content in the web/-directory, and its subdirectories.
	//Default file: index.html
	//Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
	server.on_get([&server](auto response, auto request) {
		std::string message = "Dummy";
		response->status(200).send(message);
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

	std::thread server_thread([&server]() {
		//Start server
		server.start();
	});

    
    //Wait for server to start so that the client can connect
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    //Client examples
    //Second Client() parameter set to false: no certificate verification
    HttpsClient client("localhost:8080", false);
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

