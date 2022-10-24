include(FetchContent)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
FetchContent_Declare(
        range_v3
        GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
#        GIT_TAG
)
endif()
# cmake project, fetchContent supported
FetchContent_Declare(
        refl-cpp
        GIT_REPOSITORY https://github.com/veselink1/refl-cpp.git
        GIT_TAG v0.12.3
        # PATCH_COMMAND patch -p1 -f -d <SOURCE_DIR> -i ${CMAKE_CURRENT_SOURCE_DIR}/cmake/refl-cpp.patch
        PATCH_COMMAND git config user.name 'Anonymous'
        COMMAND git config user.email '<>'
        COMMAND git am --keep-cr < ${CMAKE_CURRENT_SOURCE_DIR}/cmake/0001-refl-cpp-add-emscripten-support.patch
)

# fetch content support
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.9 # latest: v2.13.9 or v3.1.0
)

# fetch content support
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 8.1.1 # newest: 9.1.0
)

# dependency of mp-units, building examples, tests, etc is off by default
FetchContent_Declare(
        gsl-lite
        GIT_REPOSITORY https://github.com/gsl-lite/gsl-lite.git
        GIT_TAG v0.40.0
)

# prefers usage via conan, but cmake should work, but doesn't find gsl-lite in targets
FetchContent_Declare(
        mp-units
        GIT_REPOSITORY https://github.com/mpusz/units.git
        GIT_TAG v0.7.0
        SOURCE_SUBDIR src/
        # comment out find_package for gsl-lite and fmt since we use cmakeFetch for them/cmake/0001-refl-cpp-add-emscripten-support.patch
        PATCH_COMMAND git config user.name 'Anonymous'
        COMMAND git config user.email '<>'
        COMMAND git am < ${CMAKE_CURRENT_SOURCE_DIR}/cmake/0001-make-buildable-as-subproject.patch
        COMMAND git am < ${CMAKE_CURRENT_SOURCE_DIR}/cmake/0002-make-buildable-with-emscripten.patch
)

# gnutls: optional zeromq dependency for WSS (secure websockets)

# optionally required by zeromq, otherwise uses vendored tweetnacl
# FetchContent_Declare(
#         libsodium
#         GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
#         GIT_TAG 1.0.18
# )

FetchContent_Declare(
        zeromq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG v4.3.4 # latest v4.3.4
)
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "Build the tests for ZeroMQ")
option(WITH_PERF_TOOL "Build with perf-tools" OFF)

FetchContent_Declare(
        cpp-httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.11.2 # latest v0.11.2
)

# zlib: optional httplib dependency
FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.2.12 # latest v1.2.12
)

# optional dependency of httplib
FetchContent_Declare(
        openssl
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG openssl-3.0.5 # latest openssl-3.0.5
)

# mustache is forked into 3rd_party/
# FetchContent_Declare(
#         kainjow-mustache
#         GIT_REPOSITORY https://github.com/kainjow/Mustache.git
#         GIT_TAG v4.1
# )
add_library(mustache INTERFACE)
target_include_directories(mustache INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/kainjow)
add_library(mustache::mustache ALIAS mustache)

set(RXCPP_DISABLE_TESTS_AND_EXAMPLES 1)
FetchContent_Declare(
        rxcpp
        GIT_REPOSITORY https://github.com/ReactiveX/RxCpp
        GIT_TAG 7f97aa901701343593869acad1ee5a02292f39cf # TODO Change to the latest tested release of RxCpp before making a release of OpenCMW
)
FetchContent_MakeAvailable(rxcpp)

FetchContent_MakeAvailable(gsl-lite fmt refl-cpp mp-units catch2 cpp-httplib zeromq) # libsodium openssl kainjow-mustache
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib) # replace contrib by extras for catch2 v3.x.x
