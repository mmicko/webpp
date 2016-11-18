#ifndef __ASIO_WRAPPER__
#define __ASIO_WRAPPER__

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#elif defined(__GNUC__)
#if (__GNUC__ >= 5)
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif

#define ASIO_HEADER_ONLY
#define ASIO_STANDALONE
#define ASIO_SEPARATE_COMPILATION
//#define ASIO_NO_DEPRECATED
#define ASIO_NOEXCEPT noexcept(true)
#define ASIO_NOEXCEPT_OR_NOTHROW noexcept(true)
#define ASIO_ERROR_CATEGORY_NOEXCEPT noexcept(true)

#include "asio.hpp"

#endif // __ASIO_WRAPPER__