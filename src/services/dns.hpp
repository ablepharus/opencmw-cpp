#ifndef OPENCMW_CPP_DNS_HPP
#define OPENCMW_CPP_DNS_HPP

#include <atomic>
#include <majordomo/Worker.hpp>
#include "services.hpp"
#include <MIME.hpp>
#include <opencmw.hpp>
#include <refl.hpp>

namespace opencmw {
namespace service {

namespace dns {

struct Entry {
    std::string protocol;
    std::string hostname;
    int port;
    std::string service_name;
    std::string service_type;

    std::string signal_name;
    std::string signal_unit;
    float signal_rate;
    std::string signal_type;

    bool operator==(const Entry& b) const = default;
};


struct StorageEntry : Entry {
    std::chrono::time_point<std::chrono::system_clock> ttl{ std::chrono::system_clock::now() + std::chrono::hours{1}}; // kill entry if it has not been renewed before this point in time
    int id{StorageEntry::generateID()}; // some id

private:

    static int generateID() {
        static std::atomic<int> id{{}};
        return id.fetch_add(1);
    }

};

class RefreshRequest;
class DataStorage{
public:
    static DataStorage& getInstance() {
        static DataStorage instance;
        return instance;
    }

    StorageEntry addEntry(const Entry& entry) {
        std::lock_guard<std::mutex> lock(_mutex);
        StorageEntry newEntry{entry};
        _entries.push_back(newEntry);
        return newEntry;
    }

    const std::vector<StorageEntry>& getEntries() {
        return _entries;
    }

    static std::string getFilename()  {
        return {filePath};
    }
    void refreshEntry(const opencmw::service::dns::RefreshRequest &request);

protected:
    DataStorage() {
        loadDataFromFile(filePath);
        startExpirationThread();
    };
    virtual ~DataStorage() {
        _check = false;
        saveDataToFile(filePath);

        _expirationThread.join();
    }

    bool loadDataFromFile(const char* filePath);
    bool saveDataToFile(const char* filePath);
    // Disallow copy construction and assignment
    DataStorage(const DataStorage&) = delete;
    DataStorage& operator=(const DataStorage&) = delete;

    void checkExpiredEntries() {
        while (_check) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto                        now = std::chrono::system_clock::now();
                auto                        it  = std::remove_if(_entries.begin(), _entries.end(), [now](const auto &entry) {
                    auto b = entry.ttl < now;
                    return b;
                });
                _entries.erase(it, _entries.end());
            }
        }
    }
    std::thread _expirationThread;
    bool _check;
    void startExpirationThread() {
        _check = true;
        _expirationThread = std::thread(&DataStorage::checkExpiredEntries, this);
    }

    std::mutex _mutex;
    std::vector<StorageEntry> _entries;
    static constexpr char* filePath = "./dns_data_storage.yas";
};

struct Context {

    MIME::MimeType contextType{MIME::BINARY};
};

struct RegisterRequest {
    Entry entry;
};

struct RegisterResponse : Entry {
    int id;
};

struct RefreshRequest {
    int id;
};


struct QueryRequest {
    std::string protocol;
    std::string hostname;
    int port{-1};
    std::string service_name;
    std::string service_type;

    std::string signal_name;
    std::string signal_unit;
    float signal_rate{std::numeric_limits<float>::quiet_NaN()};
    std::string signal_type;

    bool operator==(const Entry& entry) const {
#define _check_string(name) \
        if (name != "" && name != entry.name) return false;

        _check_string(protocol);
        _check_string(hostname);
        _check_string(service_name);
        _check_string(service_type);
        _check_string(signal_name);
        _check_string(signal_type);
#undef _check_string
        if (port != -1 &&  port != entry.port) return false;
        if (!std::isnan(signal_rate) && signal_rate != entry.signal_rate) return false;

        return true;
    }
};

struct QueryResponse {
    std::vector<std::string> protocol;
    std::vector<std::string> hostname;
    std::vector<int> port;
    std::vector<std::string> service_name;
    std::vector<std::string> service_type;

    std::vector<std::string> signal_name;
    std::vector<std::string> signal_unit;
    std::vector<float> signal_rate;
    std::vector<std::string> signal_type;

    QueryResponse() = default;
    QueryResponse(const std::vector<Entry>& entries) {

#define _insert_into(field) \
        field.reserve(entries.size());  \
        std::transform(entries.begin(), entries.end(), std::back_inserter(field), [](const Entry& entry) {\
            return entry.field;\
        });
        _insert_into(protocol);
        _insert_into(hostname);
        _insert_into(port);
        _insert_into(service_name);
        _insert_into(service_type);
        _insert_into(signal_name);
        _insert_into(signal_unit);
        _insert_into(signal_rate);
        _insert_into(signal_type);
#undef _insert_into
    }

    std::vector<Entry> toEntries() const {
        const std::size_t size = protocol.size();
        assert(hostname.size() == size
                && port.size() == size
                && service_name.size() == size
                && service_type.size() == size
                && signal_name.size() == size
                && signal_unit.size() == size
                && signal_rate.size() == size
                && signal_type.size() == size
        );

        std::vector<Entry> res;
        res.reserve(size);

        for (std::size_t i = 0; i < size; ++i) {
            res.emplace_back(
                    protocol[i],
                    hostname[i],
                    port[i],
                    service_name[i],
                    service_type[i],
                    signal_name[i],
                    signal_unit[i],
                    signal_rate[i],
                    signal_type[i]
            );
        }

        return res;
    }
};

} // namespace dns
} // namespace service
} // opencmw

ENABLE_REFLECTION_FOR(opencmw::service::dns::Context, contextType);
ENABLE_REFLECTION_FOR(opencmw::service::dns::RegisterRequest, entry);
ENABLE_REFLECTION_FOR(opencmw::service::dns::RegisterResponse, protocol, hostname, port, service_name, service_type,
        signal_name, signal_unit, signal_rate, signal_type, id);
ENABLE_REFLECTION_FOR(opencmw::service::dns::RefreshRequest, id);
ENABLE_REFLECTION_FOR(opencmw::service::dns::Entry, protocol, hostname, port, service_name, service_type,
        signal_name, signal_unit, signal_rate, signal_type);
ENABLE_REFLECTION_FOR(opencmw::service::dns::QueryResponse, protocol, hostname, port, service_name, service_type,
        signal_name, signal_unit, signal_rate, signal_type)
ENABLE_REFLECTION_FOR(opencmw::service::dns::QueryRequest, protocol, hostname, port, service_name, service_type,
        signal_name, signal_unit, signal_rate, signal_type);


namespace opencmw {
namespace service {
namespace dns {


bool DataStorage::loadDataFromFile(const char *filePath) {
    if (!std::filesystem::exists(filePath)) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string s{buffer.str()};
    if (s.size() < 9 ) return false;
    IoBuffer ioBuffer{s.data(), s.size()};
    QueryResponse resp;
    opencmw::deserialise<opencmw::YaS, opencmw::ProtocolCheck::ALWAYS>(ioBuffer, resp);
    auto newEntries = resp.toEntries();

    std::lock_guard<std::mutex> lock(_mutex);
    std::for_each(newEntries.begin(), newEntries.end(), [&](const auto& entry) {
        _entries.push_back({entry});
    });
    return true;
}

bool DataStorage::saveDataToFile(const char *filePath) {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Entry> entries;
    std::for_each(_entries.begin(), _entries.end(), [&entries](auto& e) { entries.push_back({e});});
    QueryResponse k{entries};
    IoBuffer outBuffer;
    opencmw::serialise<opencmw::YaS>(outBuffer, k);

    std::ofstream os{filePath, std::ios::binary};
    if (!os.is_open()) return false;

    os.write((const char*)outBuffer.data(), outBuffer.size());
    return true;
}

void DataStorage::refreshEntry(const opencmw::service::dns::RefreshRequest &request) {
    std::lock_guard<std::mutex> m{_mutex};
    auto a = std::find_if(_entries.begin(), _entries.end(), [&request](auto& e) { return e.id == request.id; });
    if (a != _entries.end()) {
        a->ttl = std::chrono::system_clock::now() + std::chrono::hours{1};
    }
}

std::vector<Entry> queryServices(const QueryRequest& query = {}) {
    // serialize query parameters
    IoBuffer outBuffer;
    opencmw::serialise<opencmw::YaS>(outBuffer, query);
    std::string contentType{MIME::BINARY.typeName()};
    std::basic_string<char> s{outBuffer.asString()};


    // request
    httplib::Client client{"http://localhost:8080"};
    auto response = client.Post("QueryDNS", s, contentType);
    if (response->status > 299) throw std::runtime_error{"Server Returned error code"};

    // deserialise response
    IoBuffer buffer;
    QueryResponse queryResponse;
    buffer.put<opencmw::IoBuffer::MetaInfo::WITHOUT>(response->body);

    opencmw::deserialise<opencmw::YaS, ProtocolCheck::ALWAYS>(buffer, queryResponse);

    return queryResponse.toEntries();
}


using registerServiceWorker = majordomo::Worker<"RegisterDNS", Context, RegisterRequest, RegisterResponse,
        majordomo::description<"Register new Signals">>;

class RegisterServiceHandler {
public:
    void operator()(majordomo::RequestContext &rawCtx, const Context &ctx, const RegisterRequest &in, Context &replyContext, RegisterResponse &response) {
        auto out = DataStorage::getInstance().addEntry(in.entry);
        response = {out};
        response.id = out.id;
    }
};

using queryServiceWorker = majordomo::Worker<"QueryDNS", Context, QueryRequest, QueryResponse, majordomo::description<"QuerySignalNames">>;
class QueryServiceHandler {
public:
    void operator()(majordomo::RequestContext &rawCtx, const Context &ctx,const QueryRequest &in, Context &replyContext, QueryResponse &response) {
        auto res = DataStorage::getInstance().getEntries();
        std::vector<Entry> myres;
        auto filtered =  res | std::views::filter([&in](auto &a) { return in == a;});
        /*std::copy_if(myList.begin(), myList.end(), std::back_inserter(myres),
                [&targetName](const Entry& entry) {
                    return entry.name == targetName;
                });*/

        std::vector<Entry> result;
        std::ranges::copy(filtered, std::back_inserter(result));
        response = {result};
    }
};


RegisterResponse registerService(const Entry& entry) {
    // serialise entry
    IoBuffer outBuffer;
    opencmw::serialise<opencmw::YaS>(outBuffer, RegisterRequest{entry});
    std::string contentType{MIME::BINARY.typeName()};
    std::string body{outBuffer.asString()};

    // send request to register Service
    httplib::Client client{"http://localhost:8080", };
    auto response = client.Post("RegisterDNS", body, contentType);

    if(response.error() != httplib::Error::Success || response->status == 500) throw std::runtime_error{response->reason};

    // deserialise response
    IoBuffer inBuffer;
    inBuffer.put<opencmw::IoBuffer::MetaInfo::WITHOUT>(response->body);
    RegisterResponse res;
    try {
        opencmw::deserialise<opencmw::YaS, opencmw::ProtocolCheck::ALWAYS>(inBuffer, res);
    } catch (const ProtocolException &exc) {
        throw std::runtime_error{exc.what()}; // rethrowing, because ProtocolException behaves weird
    }

    return res;
}


using refreshServiceWorker = majordomo::Worker<"RefreshDNS", Context, RefreshRequest, RegisterResponse,
        majordomo::description<"Refresh Signal">>;
class RefreshServiceHandler {
public:
    void operator()(majordomo::RequestContext &rawCtx, const Context &ctx, const RefreshRequest &in, Context &replyContext, RegisterResponse &response) {
        DataStorage::getInstance().refreshEntry(in);
    }
};
RegisterResponse refreshService(const RefreshRequest& refresh) {
    IoBuffer outBuffer;
    opencmw::serialise<opencmw::YaS>(outBuffer, refresh);
    std::string contentType{MIME::BINARY.typeName()};
    std::string body{outBuffer.asString()};

    // send request to register Service
    httplib::Client client{"http://localhost:8080", };
    auto response = client.Post("RefreshDNS", body, contentType);

    if(response.error() != httplib::Error::Success || response->status == 500) throw std::runtime_error{response->reason};

    // deserialise response
    IoBuffer inBuffer;
    inBuffer.put<opencmw::IoBuffer::MetaInfo::WITHOUT>(response->body);
    RegisterResponse res;
    try {
        opencmw::deserialise<opencmw::YaS, opencmw::ProtocolCheck::ALWAYS>(inBuffer, res);
    } catch (const ProtocolException &exc) {
        throw std::runtime_error{exc.what()}; // rethrowing, because ProtocolException behaves weird
    }

    return res;
}

} // namespace dns
} // namespace service
} // namespace opencmw


#endif // OPENCMW_CPP_DNS_HPP
