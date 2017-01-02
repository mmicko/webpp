// license:MIT
// copyright-holders:Ole Christian Eidheim, Miodrag Milanovic
#include "server_http.hpp"
#include "client_http.hpp"
#include <fstream>
struct mapping
{
	const char* extension;
	const char* mime_type;
} mappings[] =
{
	{ "aac",     "audio/aac" },
	{ "aat",     "application/font-sfnt" },
	{ "aif",     "audio/x-aif" },
	{ "arj",     "application/x-arj-compressed" },
	{ "asf",     "video/x-ms-asf" },
	{ "avi",     "video/x-msvideo" },
	{ "bmp",     "image/bmp" },
	{ "cff",     "application/font-sfnt" },
	{ "css",     "text/css" },
	{ "csv",     "text/csv" },
	{ "doc",     "application/msword" },
	{ "eps",     "application/postscript" },
	{ "exe",     "application/octet-stream" },
	{ "gif",     "image/gif" },
	{ "gz",      "application/x-gunzip" },
	{ "htm",     "text/html" },
	{ "html",    "text/html" },
	{ "ico",     "image/x-icon" },
	{ "ief",     "image/ief" },
	{ "jpeg",    "image/jpeg" },
	{ "jpg",     "image/jpeg" },
	{ "jpm",     "image/jpm" },
	{ "jpx",     "image/jpx" },
	{ "js",      "application/javascript" },
	{ "json",    "application/json" },
	{ "m3u",     "audio/x-mpegurl" },
	{ "m4v",     "video/x-m4v" },
	{ "mid",     "audio/x-midi" },
	{ "mov",     "video/quicktime" },
	{ "mp3",     "audio/mpeg" },
	{ "mp4",     "video/mp4" },
	{ "mpeg",    "video/mpeg" },
	{ "mpg",     "video/mpeg" },
	{ "oga",     "audio/ogg" },
	{ "ogg",     "audio/ogg" },
	{ "ogv",     "video/ogg" },
	{ "otf",     "application/font-sfnt" },
	{ "pct",     "image/x-pct" },
	{ "pdf",     "application/pdf" },
	{ "pfr",     "application/font-tdpfr" },
	{ "pict",    "image/pict" },
	{ "png",     "image/png" },
	{ "ppt",     "application/x-mspowerpoint" },
	{ "ps",      "application/postscript" },
	{ "qt",      "video/quicktime" },
	{ "ra",      "audio/x-pn-realaudio" },
	{ "ram",     "audio/x-pn-realaudio" },
	{ "rar",     "application/x-arj-compressed" },
	{ "rgb",     "image/x-rgb" },
	{ "rtf",     "application/rtf" },
	{ "sgm",     "text/sgml" },
	{ "shtm",    "text/html" },
	{ "shtml",   "text/html" },
	{ "sil",     "application/font-sfnt" },
	{ "svg",     "image/svg+xml" },
	{ "swf",     "application/x-shockwave-flash" },
	{ "tar",     "application/x-tar" },
	{ "tgz",     "application/x-tar-gz" },
	{ "tif",     "image/tiff" },
	{ "tiff",    "image/tiff" },
	{ "torrent", "application/x-bittorrent" },
	{ "ttf",     "application/font-sfnt" },
	{ "txt",     "text/plain" },
	{ "wav",     "audio/x-wav" },
	{ "webm",    "video/webm" },
	{ "woff",    "application/font-woff" },
	{ "wrl",     "model/vrml" },
	{ "xhtml",   "application/xhtml+xml" },
	{ "xls",     "application/x-msexcel" },
	{ "xml",     "text/xml" },
	{ "xsl",     "application/xml" },
	{ "xslt",    "application/xml" },
	{ "zip",     "application/x-zip-compressed" }
};

std::string extension_to_type(const std::string& extension)
{
	for (mapping m : mappings)
	{
		if (m.extension == extension)
		{
			return m.mime_type;
		}
	}

	return "text/plain";
}

using HttpServer = webpp::Server<webpp::HTTP>;
using HttpClient = webpp::Client<webpp::HTTP>;

int main() {
	std::shared_ptr<asio::io_context> io_context = std::make_shared<asio::io_context>();
	//HTTP-server at port 8080 using 1 thread
	//Unless you do more heavy non-threaded processing in the resources,
	//1 thread is usually faster than several threads
	HttpServer server;
	server.m_config.port = 8080;

	server.set_io_context(io_context);

	//Add resources using path-regex and method-string, and an anonymous function
	//POST-example for the path /string, responds the posted string
	server.on_post("/string", [](auto response, auto request) {
		//Retrieve string:
		auto content=request->content.string();
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
	server.on_post("/json", [](auto response, auto /*request*/) {
		try {

			std::string name="Test Name ";
			response->type("application/json");
			response->status(200).send(name);
		}
		catch(std::exception& e) {
			response->status(400).send(e.what());
		}
	});

	//GET-example for the path /info
	//Responds with request-information
	server.on_get("/info", [](auto response, auto request) {
		std::stringstream content_stream;
		content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
		content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
		for(auto& header: request->header) {
			content_stream << header.first << ": " << header.second << "<br>";
		}

		response->status(200).send(content_stream.str());
	});

	//GET-example for the path /match/[number], responds with the matched string in path (number)
	//For instance a request GET /match/123 will receive: 123
	server.on_get("/match/:id(\\d+)", [&server](auto response, auto request) {
		std::string number = request->params["id"];
		response->status(200).send(number);
	});

	//Get example simulating heavy work in a separate thread
	server.on_get("/work", [&server](auto response, auto /*request*/) {
		std::thread work_thread([response] {
			std::this_thread::sleep_for(std::chrono::seconds(5));
			std::string message="Work done";
			response->status(200).send(message);
		});
		work_thread.detach();
	});

	//Default GET-example. If no other matches, this anonymous function will be called.
	//Will respond with content in the web/-directory, and its subdirectories.
	//Default file: index.html
	//Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
	server.on_get([&server](auto response, auto request) {

		std::string doc_root = "web/";
		std::string path = request->path;
		// If path ends in slash (i.e. is a directory) then add "index.html".
		if (path[path.size() - 1] == '/')
		{
			path += "index.html";
		}

		std::size_t last_qmark_pos = path.find_last_of("?");
		if (last_qmark_pos != std::string::npos)
			path = path.substr(0, last_qmark_pos - 1);

		// Determine the file extension.
		std::size_t last_slash_pos = path.find_last_of("/");
		std::size_t last_dot_pos = path.find_last_of(".");
		std::string extension;
		if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
		{
			extension = path.substr(last_dot_pos + 1);
		}

		// Open the file to send back.
		std::string full_path = doc_root + path;
		std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
		if (!is)
		{
			response->status(400).send("Error");
		}

		// Fill out the reply to be sent to the client.
		std::string content;
		char buf[512];
		while (is.read(buf, sizeof(buf)).gcount() > 0)
			content.append(buf, size_t(is.gcount()));

		response->type(extension_to_type(extension));
		response->status(200).send(content);

	});

	std::thread server_thread([&server, &io_context](){
		//Start server
		server.start();
		io_context->run();
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
