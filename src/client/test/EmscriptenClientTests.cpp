#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <iostream>
//
#include "Debug.hpp"
#include <emscripten/trace.h>
//#include "ClientCommon.hpp"
#include "IoBuffer.hpp"
#include "IoSerialiser.hpp"
#include "MdpMessage.hpp"
// #include "src/services/include/services/dns.hpp"
 #include "src/services/include/services/dns_client.hpp"
#include "src/services/include/services/dns_types.hpp"
#include <algorithm>
#include <chrono>

#include <IoSerialiserYaS.hpp>
#include <RestClient.hpp>
#include <future>
#include <stdio.h>
#include <string.h>
#include <emscripten/fetch.h>
#include <thread>
#include <vector>



#include "Debug.hpp"

#include "MdpMessage.hpp"
#include "RestClient.hpp"
#include <atomic>
#include <chrono>
#ifdef EMSCRIPTEN
#include <emscripten/threading.h>
#endif
#include <future>
#include <IoSerialiserYaS.hpp>
#include <thread>
#include <URI.hpp>
#include <utility>
#include <QuerySerialiser.hpp>

#define TEST_IN_MAIN
//#define TEST_IN_MAINLOOP

#define TEST_SIMPLE_REQUEST
#define TEST_SIMPLE_REQUEST_WAITABLE
#define TEST_FETCH_PROXYING
#define TEST_REST_CLIENT
#define TEST_REST_CLIENT_FUTURE
#define TEST_CONTEXTCLIENT_FUTURE
#define TEST_REST_CLIENT_SYNC

using namespace opencmw;

class TestCase {
    virtual bool hasResult{false}
    virtual void run() = 0;
    virtual bool ready() { return true; }
};

template<typename T>
struct FutureTestcase : TestCase{
    bool hasResult{true};
    std::promise<T> promise;
    std::future<T> future;
    bool ready() {
        return future.wait_for(std::chrono::seconds{0}) != std::future_status::timeout);
    }
};

std::vector<TestCase*> tests;
// std::vector<std::shared_ptr<TestCase>>
#define ADD_TESTCASE(name) tests.push_back(new name)
#ifdef CREATE_TESTCASES_STACK
#define ADD_TESTCASE(name) #name Testcase_#name; tests.push_back(Testcase_#name);
#endif



#ifdef TEST_FETCH_PROXYING
// https://emscripten.org/docs/api_reference/proxying.h.html
//  from example https://github.com/emscripten-core/emscripten/blob/main/test/pthread/test_pthread_proxying_cpp.cpp#L37
#include <emscripten/eventloop.h>
#include <emscripten/proxying.h>
#include <sched.h>

void looper_main() {
    while (!should_quit) {
        queue.execute();
        sched_yield();
    }
}

void returner_main() {
    // Return back to the event loop while keeping the runtime alive.
    // Note that we can't use `emscripten_exit_with_live_runtime` here without
    // introducing a memory leak due to way to C++11 threads interact with
    // unwinding. See https://github.com/emscripten-core/emscripten/issues/17091.
    std::cout << "Fooooo";
    emscripten_runtime_keepalive_push();
}

struct TestFetchProxying : TestCase {
    std::thread       looper;
    std::thread       returner;
    std::atomic<bool> should_quit{ false };

    // The queue used to send work to both `looper` and `returner`.
    emscripten::ProxyingQueue queue;

    TestFetchProxying() {
        std::thread{ []() {
            sample_request();
        } }.detach();

        // Proxy to looper.
        {
            queue.proxyAsync(looper.native_handle(), [&]() { sample_request(); });
        }

        // Proxy to returner.
        {
            queue.proxyAsync(returner.native_handle(), [&]() { sample_request(); });
        }
    }
};
ADD_TESTCASE(TestFetchProxying)

#endif


#ifdef TEST_SIMPLE_REQUEST

void downloadSucceeded(emscripten_fetch_t *fetch) {
     printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
     // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
     emscripten_fetch_close(fetch); // Free data associated with the fetch.
 }

void downloadFailed(emscripten_fetch_t *fetch) {
    printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
     emscripten_fetch_close(fetch); // Also free data on failure.
}

struct TestEmscriptenRequest : TestCase {
     void run() {
        DEBUG_LOG("requesting");
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.onsuccess  = downloadSucceeded;
        attr.onerror    = downloadFailed;
        emscripten_fetch(&attr, "http://localhost:8055/dns");
     }
};
ADD_TESTCASE(TestEmscriptenRequest);

#endif

#ifdef TEST_SIMPLE_REQUEST_WAITABLE
struct TestEmscriptenWaitableRequest {
     void run() {
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_WAITABLE;
        // attr.onsuccess = downloadSucceeded;
        // attr.onerror = downloadFailed;
        auto              fetch = emscripten_fetch(&attr, "http://localhost:8055/dns");

        EMSCRIPTEN_RESULT ret   = EMSCRIPTEN_RESULT_TIMED_OUT;
        while (ret == EMSCRIPTEN_RESULT_TIMED_OUT) {
            ret = emscripten_fetch_wait(fetch, 2000 /*milliseconds to wait, 0 to just poll, INFINITY=wait until completion*/);
        }
        std::cout << (fetch->numBytes > 0 ? "waitable works!" : "waitable does not work") << std::endl;
     }
}
ADD_TESTCASE(TestEmscriptenWaitableRequest)
#endif // TEST_SIMPLE_REQUEST_WAITABLE

#define TEST_REST_CLIENT_MANUAL
#ifdef TEST_REST_CLIENT_MANUAL
struct TestRestClientManual : TestCase {
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     void                        run() {
        client::Command getRequest;
        getRequest.command  = mdp::Command::Get;
        getRequest.endpoint = URI<>{ "http://localhost:8055/dns" };
        getRequest.callback = [](const mdp::Message &reply) {
            if (reply.data.size() > 300 && reply.error == "") {
                std::cout << "getRequest success" << std::endl;
            }
        };
        r.request(getRequest);
     }
};
ADD_TESTCASE(TestRestClientManual)
#endif // TEST_REST_CLIENT_MANUAL

#ifdef TEST_REST_CLIENT

struct TestRestClient : FutureTestcase<std::vector<service::dns>> {
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     void run() {
        future = r.querySignalsListFuture(promise)
     }
     std::promise<service::dns::FlatEntryList> rest_client_promise;
     auto p = r.querySignalsListFuture(rest_client_promise);
     return p;
 }
ADD_TESTCASE(TestRestClient)
#endif


#ifdef TEST_REST_CLIENT_FUTURE
 struct TestRestClientFuture : FutureTestcase<service::dns::FlatEntryList> {
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     TestRestClientFuture() {
        future = r.querySignalsListFuture(promise, {});
     }
 };
 ADD_TESTCASE(TestRestClientFuture)
#endif // TEST_REST_CLIENT_FUTURE

#ifdef TEST_REST_CLIENT_SYNC
 struct TestRestClientSync : FutureTestcase<service::dns::FlatEntryList> {
     void run() {
        service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
        promise.set_value(r.querySignals());
     }
 };
ADD_TESTCASE(TestRestClient)
#endif


#ifdef TEST_CONTEXTCLIENT_FUTURE
struct TestContextClient : FutureTestcase<service::dns::Entry> {
    std::vector<std::unique_ptr<opencmw::client::ClientBase>> clients{std::make_unique<client::RestClient>(opencmw::client::DefaultContentTypeHeader(MIME::BINARY))};
    //clients.emplace_back();
    client::ClientContext   clientContext{ std::move(clients) };
    service::dns::DnsClient dnsclient{ clientContext, URI<>{ "http://localhost:8055/dns" } };

    void run() {
        std::vector<service::dns::Entry> signals;

        signals = dnsclient.querySignals();
        clientContext.stop();
        promise.set_value(signals);
    }

}};
ADD_TESTCASE(TestContextClient)
#endif


void wait_for_results() {
     static bool done_once{false};
     if (!done_once) {
        sample_request_waitable();
        sample_request();
     }

     bool ready{true};

     //std::all_of(tests.begin(), tests.end(), [](std::weak_ptr?))
     ready = std::all_of(tests.begin(), tests.end(), [](TestCase* tc) {
        return tc->ready();
     });

    if (ready) {
#if defined(TEST_IN_MAINLOOP) & defined(TEST_MAINLOOP_CANCEL)
        emscripten_cancel_main_looop();
//        emscripten_force_exit(0);
//        exit(0);
//        emscripten_cancel_main_loop();
#else
        exit(0);
#endif
    }
}

int main() {
    emscripten_trace_configure_for_google_wtf();

    /*while (true) {

    }*/

#ifdef TEST_IN_MAINLOOP
    emscripten_set_main_loop(wait_for_results, 30, 0);
    DEBUG_LOG("main loop set")
#else
    sample_request();
    emscripten_pause_main_loop();
    DEBUG_LOG("CONTINUEING after pausing main loop")

    //    while (true) {
    //        wait_for_results();
//    emscripten_main_thread_process_queued_calls();
//    emscripten_current_thread_process_queued_calls();
//    emscripten_thread_sleep(500);
//    //emscripten_sleep(500);
//    std::this_thread::yield();
//    sched_yield();
    //    }
#endif

    DEBUG_LOG("~main")
}



// emscripten_pause_main_loop  ?