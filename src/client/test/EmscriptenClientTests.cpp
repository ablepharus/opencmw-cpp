#ifdef EMSCRIPTEN
#include <emscripten/threading.h>
#include <emscripten/emscripten.h>
#include <emscripten/trace.h>
#endif

#include <iostream>
#include <future>
#include <utility>
#include <algorithm>
#include <chrono>

#include "src/services/include/services/dns_client.hpp"
#include "src/services/include/services/dns_types.hpp"
#include "Debug.hpp"


#define TEST_IN_MAIN
#define TEST_IN_MAINLOOP

//#define TESTS_RUN_IN_THREADS
//#define RUN_TESTS_BEFORE_MAIN

#define TEST_SIMPLE_REQUEST
#define TEST_SIMPLE_REQUEST_WAITABLE
#define TEST_FETCH_PROXYING
#define TEST_REST_CLIENT_MANUAL
#define TEST_REST_CLIENT
#define TEST_REST_CLIENT_FUTURE
#define TEST_CONTEXTCLIENT_FUTURE
#define TEST_REST_CLIENT_SYNC

using namespace opencmw;
struct TestCase;

static std::vector<TestCase*> tests;

struct TestCase {
    TestCase(std::string testname = "test"):name(testname){
        tests.push_back(this);
    }

    virtual void _run() = 0;
    void run() {
        DEBUG_LOG("Running " + name);
        _run();
        DEBUG_LOG(name + " Finished")
    }
    virtual bool ready() { return true; }
    virtual ~TestCase() = default;
    std::string name;
};

template<typename T>
struct FutureTestcase : TestCase{
    using TestCase::TestCase;
    std::promise<T> promise;
    std::future<T> future;
    bool ready() override {
        return future.wait_for(std::chrono::seconds{0}) != std::future_status::timeout;
    }
};

// std::vector<std::shared_ptr<TestCase>>

//#define ADD_TESTCASE(testcasetype) testcasetype _##testcasetype( #testcasetype );
#define ADD_TESTCASE(testcasetype) testcasetype _##testcasetype{ #testcasetype };

#ifdef CREATE_TESTCASES_ON_STACK
#define ADD_TESTCASE(name) #name Testcase_#name; tests.push_back(Testcase_#name);
#endif

void run_tests() {
    std::for_each(tests.begin(), tests.end(), [](TestCase* t) {
        DEBUG_LOG(t->name);
#ifdef TESTS_RUN_IN_THREADS
        std::thread{ [t]() { t->run(); } }.detach();
#else
                t->run();
                std::cout << "ready " << t->ready() << std::endl;
#endif
    });
}


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
     using TestCase::TestCase;

     std::atomic_bool dataReady{ false };

     bool ready() override {
         return dataReady.load();
     }

     void _run() override {
        DEBUG_LOG("requesting");
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.onsuccess = TestEmscriptenRequest::downloadSucceeded;
        attr.onerror = TestEmscriptenRequest::downloadFailed;
        attr.userData = this;
        emscripten_fetch(&attr, "http://localhost:8055/dns");
     }

     static void downloadSucceeded(emscripten_fetch_t* fetch) {
        TestEmscriptenRequest* self = static_cast<TestEmscriptenRequest*>(fetch->userData);
        // Process fetch results...
        self->dataReady.store(true);
        //self->cv.notify_one();

        printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
        // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
        emscripten_fetch_close(fetch); // Free data associated with the fetch.
     }

     static void downloadFailed(emscripten_fetch_t* fetch) {
        printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
        // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
        emscripten_fetch_close(fetch); // Free data associated with the fetch.
     }
};
//
//struct TestEmscriptenRequest : TestCase {
//     using TestCase::TestCase;
//     // std::atomic_bool... (false)
//     // std::conditional variable
//     bool ready() override {
//        // do a wait_util on conditional variable, if the value is available to load, return true
//        //  unless ready is false?   there can be problems with deadlocks on atamics, that why the
//        // check on the cv, does that make sense? or do we just run into the deadlock when loading
//        // the value of the atomic, because the cv only says it's accessible, not that it is ready
//     }
//     void _run() override{
//        DEBUG_LOG("requesting");
//        emscripten_fetch_attr_t attr;
//        emscripten_fetch_attr_init(&attr);
//        strcpy(attr.requestMethod, "GET");
//        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
//        attr.onsuccess  = downloadSucceeded;
//        attr.onerror    = downloadFailed;
//        attr.userData = this;
//        emscripten_fetch(&attr, "http://localhost:8055/dns");
//     }
//};
ADD_TESTCASE(TestEmscriptenRequest)

#endif

#ifdef TEST_FETCH_PROXYING
// https://emscripten.org/docs/api_reference/proxying.h.html
//  from example https://github.com/emscripten-core/emscripten/blob/main/test/pthread/test_pthread_proxying_cpp.cpp#L37
#include <emscripten/eventloop.h>
#include <emscripten/proxying.h>
#include <sched.h>

emscripten::ProxyingQueue queue;

std::atomic<bool> should_quit{false};
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
     emscripten_runtime_keepalive_push();
}

struct TestFetchProxying : TestCase {
     using TestCase::TestCase;
     std::thread       looper;
     std::thread       returner;
     std::atomic<bool> should_quit{ false };
     std::atomic_bool looper_done{false};
     std::atomic_bool returner_done{false};

     // The queue used to send work to both `looper` and `returner`.
     TestEmscriptenRequest t;

     bool ready() {
        return should_quit && looper_done && returner_done;
     }

     void _run () {
//        t.run();
        std::thread q{ [this]() {
            t.run();
        } };

        // Proxy to looper.
        {
            queue.proxyAsync(looper.native_handle(), [&]() { t.run(); looper_done = true; });
        }

        // Proxy to returner.
        {
            queue.proxyAsync(returner.native_handle(), [&]() { t.run(); returner_done = true; });
        }
        q.join();
     }
};
ADD_TESTCASE(TestFetchProxying)

#endif

#ifdef TEST_SIMPLE_REQUEST_WAITABLE
struct TestEmscriptenWaitableRequest : public TestCase {
     using TestCase::TestCase;
     EMSCRIPTEN_RESULT ret   = EMSCRIPTEN_RESULT_TIMED_OUT;
     emscripten_fetch_t *fetch;

     void _run() {
        std::thread{ [this]() {
            emscripten_fetch_attr_t attr;
            emscripten_fetch_attr_init(&attr);
            strcpy(attr.requestMethod, "GET");
            attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_WAITABLE;
            // attr.onsuccess = downloadSucceeded;
            // attr.onerror = downloadFailed;
            fetch = emscripten_fetch(&attr, "http://localhost:8055/dns");


            if (ready())
                std::cout << (fetch->numBytes > 0 ? "waitable works!" : "waitable does not work") << std::endl;
        } }.detach();
     }
     bool ready() {
        return emscripten_fetch_wait(fetch, 0) != EMSCRIPTEN_RESULT_TIMED_OUT;
     }
};
ADD_TESTCASE(TestEmscriptenWaitableRequest)
#endif // TEST_SIMPLE_REQUEST_WAITABLE


#ifdef TEST_REST_CLIENT_MANUAL
struct TestRestClientManual : FutureTestcase<bool> {
     using FutureTestcase<bool>::FutureTestcase;
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     void                        _run() {
        future = promise.get_future();
        client::Command getRequest;
        getRequest.command  = mdp::Command::Get;
        getRequest.endpoint = URI<>{ "http://localhost:8055/dns" };
        getRequest.callback = [this](const mdp::Message &reply) {
            DEBUG_LOG("")
            if (reply.data.size() > 300 && reply.error == "") {
                std::cout << "getRequest success" << std::endl;
                promise.set_value(true);
            } else {
                promise.set_value(false);
            }
        };
        r.request(getRequest);
     }
};
ADD_TESTCASE(TestRestClientManual)
#endif // TEST_REST_CLIENT_MANUAL

#ifdef TEST_REST_CLIENT

struct TestRestClient : FutureTestcase<service::dns::FlatEntryList> {
     using FutureTestcase<service::dns::FlatEntryList>::FutureTestcase;
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     void _run() {
        future = r.querySignalsListFuture(promise);
     }
};
ADD_TESTCASE(TestRestClient)
#endif


#ifdef TEST_REST_CLIENT_FUTURE
 struct TestRestClientFuture : FutureTestcase<service::dns::FlatEntryList> {
     using FutureTestcase<service::dns::FlatEntryList>::FutureTestcase;
     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
     void _run() {
        future = r.querySignalsListFuture(promise, {});
     }
 };
 ADD_TESTCASE(TestRestClientFuture)
#endif // TEST_REST_CLIENT_FUTURE

#ifdef TEST_REST_CLIENT_SYNC
 struct TestRestClientSync : FutureTestcase<service::dns::FlatEntryList> {
     using FutureTestcase<service::dns::FlatEntryList>::FutureTestcase;
     void _run() override {
        service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
        promise.set_value(r.querySignals());
     }
 };
ADD_TESTCASE(TestRestClientSync)
#endif

#ifdef TEST_CONTEXTCLIENT_FUTURE
struct TestContextClient : FutureTestcase<std::vector<service::dns::Entry>> {
    using FutureTestcase<std::vector<service::dns::Entry>>::FutureTestcase;
    std::unique_ptr<client::ClientContext>   clientContext;
    std::unique_ptr<service::dns::DnsClient> dnsClient;

    TestContextClient(std::string name)
            :FutureTestcase<std::vector<service::dns::Entry>>(name)
    {
        std::vector<std::unique_ptr<opencmw::client::ClientBase>> clients;
        clients.emplace_back(std::make_unique<client::RestClient>(opencmw::client::DefaultContentTypeHeader(MIME::BINARY)));
        clientContext = std::make_unique<client::ClientContext>(std::move(clients));
        dnsClient = std::make_unique<service::dns::DnsClient>(*clientContext.get(), URI<>{ "http://localhost:8055/dns" });

        future = promise.get_future();
    }

    void _run() {
        std::vector<service::dns::Entry> signals;

        DEBUG_FINISH(signals = dnsClient.get()->querySignals();)

        clientContext->stop();
        promise.set_value(signals);
    }

};
ADD_TESTCASE(TestContextClient)
#endif


void wait_for_results() {
    static bool run_once{false};  // in case we run tests without result
    if (!run_once) {
        run_once = true;
        //run_tests();
        return;
    }
    DEBUG_LOG_EVERY_SECOND("waiting for results");

     bool ready = std::all_of(tests.begin(), tests.end(), [](TestCase* t) {
        return t->ready();
     });

    if (ready) {
        DEBUG_LOG("~wait_for_results")
#ifndef EXIT_RUNTIME
        emscripten_run_script("if (serverOnExit) serverOnExit();");
#endif

#if defined(TEST_IN_MAINLOOP) & defined(TEST_MAINLOOP_CANCEL)
        emscripten_cancel_main_looop();
//        emscripten_force_exit(0);
//        exit(0);
//        emscripten_cancel_main_loop();
#else
        emscripten_force_exit(0);
#endif
    }
}

bool ayokay = std::all_of(tests.begin(), tests.end(), [](auto a ) {return 0;});
int main(int argc, char* argv[]) {
    emscripten_trace_configure_for_google_wtf();

    if (argc > 1) {  // only execute selected Tests
        auto r = std::remove_if(tests.begin(), tests.end(), [argv, argc](TestCase *t) {
            return std::all_of(argv + 1, argv + argc, [t](auto e){ return e != t->name; });
        });
        tests.erase(r, tests.end());
    }

#ifdef EXIT_RUNTIME
    emscripten_run_script("addOnExit(serverOnExit);");
#endif

#ifdef TEST_IN_MAINLOOP_
    //emscripten_set_main_loop(wait_for_results, 30, 0);
    DEBUG_LOG("main loop set")
    // we are running the tests here. they may not wait for fetch results, because
    // we have to return from main to put the new main loop in effect
    run_tests();
#else
    run_tests();

    while (std::any_of(tests.begin(), tests.end(),
            [](TestCase *t) { return !t->ready(); })) {
        wait_for_results();
        emscripten_pause_main_loop();
        emscripten_main_thread_process_queued_calls();
        emscripten_current_thread_process_queued_calls();
//        emscripten_thread_sleep(500);
//        // emscripten_sleep would help, but this requires ASYNCIFY, which is incompatible with exceptions
//        // emscripten_sleep(500);
        std::this_thread::yield();
        sched_yield();
    }
#endif

    // We have to exit the main thread
    DEBUG_LOG("~main")
}



// emscripten_pause_main_loop  ?