
#define CATCH_CONFIG_MAIN // This tells the catch header to generate a main
#define CATCH_CONFIG_ENABLE_BENCHMARKING 1

#include <string>
#include <filesystem>
class FileDeleter {
public:
    // make sure to delete datastorage file when finishing
    FileDeleter(std::string filename = "dns_data_storage.yas"):filename(filename) {
        deleteFile();
    }
    ~FileDeleter() {
        deleteFile();
    }
    void deleteFile() {
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }
    }
    std::string filename;
};

FileDeleter fd; // delete DataStorage file when finishing
#include "../dns.hpp"
#include <catch2/catch.hpp>

using namespace opencmw::service;
using namespace opencmw::services;

dns::Entry a{"http", "localhost", 8080, "test", "unknown", "A", "ms", 300, ""};
dns::Entry b{"http", "localhost", 8080, "test", "unknown", "B", "ms", 300, ""};
dns::Entry c{"http", "localhost", 8080, "test", "unknown", "C", "ms", 300, ""};


class TestDataStorage : public opencmw::service::dns::DataStorage {
public:
    void clear() {
        _entries.clear();
    }

    bool save(const char* path) {
        return saveDataToFile(path);
    }

    bool load(const char* path) {
        return loadDataFromFile(path);
    }

    std::vector<opencmw::service::dns::StorageEntry>& entries() {
        return _entries;
    }
    TestDataStorage() {
        clear();
    }
    ~TestDataStorage() {}
};


TEST_CASE("type tests", "[DNS") {

    SECTION("Type Conversions") {
        std::vector<dns::Entry> entries{a, b, c};
        dns::QueryResponse response{{a, b, c}};
        auto newEntries = response.toEntries();
        REQUIRE(newEntries[0] == a);
        REQUIRE(newEntries[1] == b);
        REQUIRE(newEntries[2] == c);
    }
    SECTION("StorageEntry") {
        opencmw::service::dns::StorageEntry sE{a}, sE2{b}, sE3{c};
        REQUIRE(sE.id == 0); REQUIRE(sE.signal_name == "A");
        REQUIRE(sE2.id == 1); REQUIRE(sE2.signal_name == "B");
        REQUIRE(sE3.id == 2); REQUIRE(sE3.signal_name == "C");

        std::vector<opencmw::service::dns::StorageEntry> entries;
        entries.push_back(sE);
        entries.push_back(sE2);
        entries.push_back(sE3);
        REQUIRE(entries[0].id == 0); REQUIRE(entries[0].signal_name == "A");
        REQUIRE(entries[1].id == 1); REQUIRE(entries[1].signal_name == "B");
        REQUIRE(entries[2].id == 2); REQUIRE(entries[2].signal_name == "C");
    }
}

TEST_CASE("run services", "[DNS]") {
    opencmw::services::RunDefaultBroker broker;
    broker.runWorker<opencmw::service::dns::registerServiceWorker, opencmw::service::dns::RegisterServiceHandler>();
    broker.runWorker<opencmw::service::dns::queryServiceWorker, opencmw::service::dns::QueryServiceHandler>();
    broker.startBroker();
}

TEST_CASE("service", "[DNS]") {

    opencmw::services::RunDefaultBroker broker;
    broker.runWorker<opencmw::service::dns::queryServiceWorker, opencmw::service::dns::QueryServiceHandler>();
    broker.startBroker();

    SECTION("basic query") {
        auto services = dns::queryServices();
        REQUIRE(services.size() == 0);
        opencmw::service::dns::DataStorage::getInstance().addEntry(a);
        services = dns::queryServices();
        REQUIRE(services.size() == 1);
        REQUIRE(services.at(0) == a);
        opencmw::service::dns::DataStorage::getInstance().addEntry(b);
        services = dns::queryServices();
        //REQUIRE(services == opencmw::service::dns::DataStorage::getInstance().getEntries());
        auto entries = opencmw::service::dns::DataStorage::getInstance().getEntries();
        REQUIRE(services.size() == entries.size());
        for (int i = 0; i < services.size(); i++) {
            REQUIRE(services[i] == entries[i]);
        }
    }

    SECTION("more interesting query") {
        auto services = dns::queryServices({ .signal_name = "C" });
        REQUIRE(services.size() == 0);
        opencmw::service::dns::DataStorage::getInstance().addEntry(c);
        services = dns::queryServices({ .signal_name = "C" });
        REQUIRE(services.size() == 1);
        REQUIRE(services[0] == c);
    }
}

TEST_CASE("registering", "[DNS]") {
    opencmw::services::RunDefaultBroker broker;
    broker.runWorker<opencmw::service::dns::registerServiceWorker, opencmw::service::dns::RegisterServiceHandler>();
    broker.runWorker<opencmw::service::dns::refreshServiceWorker , opencmw::service::dns::RefreshServiceHandler>();
    broker.startBroker();

    auto& dataStorage = opencmw::service::dns::DataStorage::getInstance();
    auto& services = dataStorage.getEntries();
    SECTION("registering service") {
        auto startCount = services.size();

        opencmw::service::dns::registerService(a);
        REQUIRE(services.size() == startCount + 1);
        REQUIRE(services[startCount] == a);
    }
    SECTION("unregistering service when not reregistered") {
        TestDataStorage ds;
        ds.addEntry(a);
        ds.addEntry(b);
        ds.addEntry(c);
        auto& entries = ds.entries();

        entries[0].ttl = std::chrono::system_clock::now();
        entries[2].ttl = std::chrono::system_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds{12});
        REQUIRE(entries.size() == 1);
        REQUIRE(entries[0] == b);
    }
    SECTION("reregistering service") {
        TestDataStorage ds;
        ds.addEntry(a);
        ds.addEntry(b);
        ds.addEntry(c);
        auto& entries = ds.entries();

        entries[0].ttl = std::chrono::system_clock::now();
        entries[2].ttl = std::chrono::system_clock::now();
        auto id = entries[2].id;
        ds.refreshEntry({id});
        std::this_thread::sleep_for(std::chrono::seconds{12});
        REQUIRE(entries.size() == 2);
        REQUIRE(entries[1].id == id);
    }

}

TEST_CASE("datastorage persistence", "[DNS]") {
    SECTION("One Entry") {
        const char* filename = "test1.yas";
        FileDeleter fd{filename};
        TestDataStorage ds;
        ds.addEntry(a);
        REQUIRE(ds.save(filename));

        REQUIRE(std::filesystem::exists(filename));
        ds.clear();
        REQUIRE(ds.getEntries().size() == 0);
        auto bret = ds.load(filename);
        REQUIRE(bret);
        REQUIRE(ds.getEntries().size() == 1);
    }
    SECTION("Three Entries") {
        const char* filename = "test2.yas";
        FileDeleter fd{filename};
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }
        TestDataStorage ds;
        ds.addEntry(a);
        ds.addEntry(b);
        ds.addEntry(c);
        REQUIRE(ds.save(filename));

        REQUIRE(std::filesystem::exists(filename));
        ds.clear();
        REQUIRE(ds.getEntries().size() == 0);
        auto bret = ds.load(filename);
        REQUIRE(bret);
        REQUIRE(ds.getEntries().size() == 3);
        auto entries = ds.getEntries();
        REQUIRE(entries[0] == a);
        REQUIRE(entries[1] == b);
        REQUIRE(entries[2] == c);
    }
}