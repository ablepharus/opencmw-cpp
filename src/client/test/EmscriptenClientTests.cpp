#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <iostream>
//
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

#include <future>
#include <stdio.h>
#include <string.h>
#include <emscripten/fetch.h>
#include <thread>
#include <vector>

// void downloadSucceeded(emscripten_fetch_t *fetch) {
//     printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
//     // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
//     emscripten_fetch_close(fetch); // Free data associated with the fetch.
// }
//
// void downloadFailed(emscripten_fetch_t *fetch) {
//     printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
//     emscripten_fetch_close(fetch); // Also free data on failure.
// }
//
// using namespace opencmw;
// void sample_request() {
//     emscripten_fetch_attr_t attr;
//     emscripten_fetch_attr_init(&attr);
//     strcpy(attr.requestMethod, "GET");
//     attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
//     attr.onsuccess  = downloadSucceeded;
//     attr.onerror    = downloadFailed;
//     emscripten_fetch(&attr, "http://localhost:8055/dns");
// }
//
// void sample_request_waitable() {
//     emscripten_fetch_attr_t attr;
//     emscripten_fetch_attr_init(&attr);
//     strcpy(attr.requestMethod, "GET");
//     attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_WAITABLE;
//     // attr.onsuccess = downloadSucceeded;
//     // attr.onerror = downloadFailed;
//     auto              fetch = emscripten_fetch(&attr, "http://localhost:8055/dns");
//
//     EMSCRIPTEN_RESULT ret   = EMSCRIPTEN_RESULT_TIMED_OUT;
//     while (ret == EMSCRIPTEN_RESULT_TIMED_OUT) {
//         ret = emscripten_fetch_wait(fetch, 2000 /*milliseconds to wait, 0 to just poll, INFINITY=wait until completion*/);
//     }
//     std::cout << (fetch->numBytes > 0 ? "waitable works!" : "waitable does not work") << std::endl;
// }
//
// auto simple_rest_client() {
//     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
//     client::Command             getRequest;
//     getRequest.command  = mdp::Command::Get;
//     getRequest.endpoint = URI<>{ "http://localhost:8055/dns" };
//     getRequest.callback = [](const mdp::Message &reply) {
//         if (reply.data.size() > 300 && reply.error == "") {
//             std::cout << "getRequest success" << std::endl;
//         }
//     };
//     // sample_request_waitable();
//     // r.request(getRequest);
//     //auto p = r.querySignalsListFuture({});
//
//     //return p;
//
//     //    fetch->data
//     // std::cout <<"simple_rest_client signals.size" << signals.size() << std::endl;
//     // std::cout << r.querySignals().size() << std::endl;
//     std::cout << "done" << std::endl;
// }
//
//

// struct TestCase {
//     std::future<bool> future;
// };
// struct PromiseTestcase : TestCase{
//     service::dns::DnsRestClient r{ "http://localhost:8055/dns" };
//     std::promise<service::dns::FlatEntryList> promise;
//     std::future<service::dns::FlatEntryList> future;
//     PromiseTestcase() {
//      //   future = r.querySignalsListFuture(promise, {});
//     }
//
// }ptC;
//
//
//std::vector<TestCase*> tests{&ptC};
//std::promise<std::vector<service::dns::Entry>> threadPromise;
//std::future<std::vector<service::dns::Entry>> threadResult;
//std::thread t{[]() {
//    service::dns::DnsRestClient r{"http://localhost:8055/dns"};
//    threadPromise.set_value(r.querySignals());
//}};
std::promise<std::vector<service::dns::Entry>> threadPromiseClientContext;
std::future<std::vector<service::dns::Entry>> threadResultClientContext = threadPromiseClientContext.get_future();
std::thread tClientContext{[]() {
    std::vector<std::unique_ptr<opencmw::client::ClientBase>> clients;
    clients.emplace_back(std::make_unique<client::RestClient>(opencmw::client::DefaultContentTypeHeader(MIME::BINARY)));
    client::ClientContext clientContext{ std::move(clients) };
    service::dns::DnsClient dnsclient{clientContext, URI<>{"http://localhost:8055/dns"}};
    auto signals = dnsclient.querySignals();
    threadPromiseClientContext.set_value(signals);
    clientContext.stop();
}};
void wait_for_results() {
//     bool done = std::all_of(tests.begin(), tests.end(), [](auto *t) {
//         return t->wait_until(std::chrono::system_clock::now()) == std::future_status::ready;
//     });

    // exit(std::all_of(tests.begin(), tests.end()), [](auto& f){return f.get();});
    // std::cout << ptC.future.valid() << std::endl;
    // std::cout << (ptC.future.wait_until(std::chrono::system_clock::now()) == std::future_status::ready);
//     if (t.joinable()) {
//         t.join();
//         emscripten_cancel_main_loop();
//     }
     /*if (threadResult.wait_until(std::chrono::system_clock::now()) == std::future_status::ready) {
         emscripten_cancel_main_loop();
     }*/
     if (threadResultClientContext.wait_until(std::chrono::system_clock::now()) == std::future_status::ready) {
         emscripten_cancel_main_loop();
     }
//     if (ptC.future.wait_until(std::chrono::system_clock::now()) == std::future_status::ready) {
//        std::cout << "ready" << std::endl;
//        std::cout << ptC.future.get().signal_unit.size() << std::endl;
//        std::cout << "bybye"<< std::endl;
//
//        emscripten_cancel_main_loop();
//     }
    std::cout << "running" << std::endl;
}


int main() {
//threadResultClientContext.get();
        while (threadResultClientContext.wait_until(std::chrono::system_clock::now()) != std::future_status::ready) {
        }
    std::cout << "result:" << threadResultClientContext.get().size() << std::endl;

    //emscripten_cancel_main_loop();
         emscripten_force_exit(0);
         exit(0);

         emscripten_cancel_main_loop();


    emscripten_trace_configure_for_google_wtf();
    // sample_request();
    //auto p = simple_rest_client();

    // future from a packaged_task
    //std::packaged_task<bool()> task([]{ return false; }); // wrap the function
    //std::future<bool> f1 = task.get_future();  // get a future
    //tests.push_back(std::move(f1));
    //auto d = p.get_future().get();
    //p.get_future().wait_until(std::chrono::system_clock::now()+ std::chrono::microseconds{500});
    //
//    PromiseTestcase mtc;
//  mtc.wait() would work here
    /*while (true) {
        emscripten_main_thread_process_queued_calls();
        emscripten_current_thread_process_queued_calls();
        emscripten_thread_sleep(500);
        //emscripten_sleep(500);
//        if (mtc.future.wait_until(std::chrono::system_clock::now()) == std::future_status::ready) {
//            break;
        //}
    }*/
    //ptC.future.get();
    std::cout << "exit" << std::endl;
    //wait_for_results();
    emscripten_set_main_loop(wait_for_results, 30, 1);
    std::cout << "real_exit?" << std::endl;
    //emscripten_force_exit(0);

}