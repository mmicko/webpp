cmake_minimum_required(VERSION 3.0)
project(webpp)
include_directories(include)
include_directories(3rdparty/asio/include)
include_directories(3rdparty/path_to_regex)

find_package(Threads REQUIRED)

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()

#TODO: add requirement for version 1.0.1g (can it be done in one line?)
find_package(OpenSSL)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "--std=c++14 ${CMAKE_CXX_FLAGS}")
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++14 ${CMAKE_CXX_FLAGS}")
endif ()

if( MSYS OR MINGW)
  # Work around 'too many sections' error with MSYS/GCC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()

set(HTTP_HEADERS  include/asio.h include/server_http.hpp  include/client_http.hpp  3rdparty/path_to_regex/path_to_regex.hpp)
set(HTTPS_HEADERS include/asio.h include/server_https.hpp include/client_https.hpp 3rdparty/path_to_regex/path_to_regex.hpp)

set(WS_HEADERS  include/asio.h include/sha1.hpp include/crypto.hpp include/base64.hpp include/server_ws.hpp  include/client_ws.hpp  3rdparty/path_to_regex/path_to_regex.hpp)
set(WSS_HEADERS include/asio.h include/sha1.hpp include/crypto.hpp include/base64.hpp include/server_wss.hpp include/client_wss.hpp 3rdparty/path_to_regex/path_to_regex.hpp)

if(OPENSSL_FOUND)
    include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

    add_executable(https_examples https_examples.cpp 3rdparty/path_to_regex/path_to_regex.cpp ${HTTPS_HEADERS})
    target_link_libraries(https_examples ${OPENSSL_LIBRARIES})
    target_link_libraries(https_examples ${CMAKE_THREAD_LIBS_INIT})

    add_executable(wss_examples wss_examples.cpp 3rdparty/path_to_regex/path_to_regex.cpp ${WSS_HEADERS})
    target_link_libraries(wss_examples ${OPENSSL_LIBRARIES})
    target_link_libraries(wss_examples ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable(http_examples http_examples.cpp 3rdparty/path_to_regex/path_to_regex.cpp ${HTTP_HEADERS})
target_link_libraries(http_examples ${CMAKE_THREAD_LIBS_INIT})

add_executable(ws_examples ws_examples.cpp 3rdparty/path_to_regex/path_to_regex.cpp ${WS_HEADERS})
target_link_libraries(ws_examples ${CMAKE_THREAD_LIBS_INIT})

if( MSYS OR MINGW OR MSVC) #TODO: Is MSYS true when MSVC is true?
    target_link_libraries(http_examples ws2_32 wsock32)
    target_link_libraries(ws_examples ws2_32 wsock32)
	if(OPENSSL_FOUND)
		target_link_libraries(https_examples ws2_32 wsock32)
		target_link_libraries(wss_examples ws2_32 wsock32)
	endif()	
endif()