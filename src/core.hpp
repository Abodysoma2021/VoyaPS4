#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace voya {
constexpr uint64_t max_package_size = 512ULL * 1024 * 1024 * 1024;
struct Error : std::runtime_error {
    bool retryable;
    Error(const std::string& message, bool retry = false)
        : std::runtime_error(message), retryable(retry) {}
};
struct Item {
    std::string name, url, sha256, description, source;
    uint64_t size = 0;
    bool update = false;
    std::string id() const;
};
struct Response {
    int status = 0;
    std::map<std::string, std::string> headers;
    virtual ~Response() = default;
    virtual size_t read(void* buffer, size_t size) = 0;
    std::string header(const std::string& name) const;
};
struct Http {
    virtual ~Http() = default;
    virtual std::unique_ptr<Response> open(const std::string& url, uint64_t offset,
                                          const std::string& validator) = 0;
};
std::unique_ptr<Http> make_http();
std::string sha256(const std::string& text);
std::string file_sha256(const std::string& path, const std::function<bool()>& stop = {});
bool valid_url(const std::string& url, bool https_only = false);
bool newer_version(const std::string& candidate, const std::string& current);
void validate_item(const Item& item);
std::vector<Item> parse_catalog(const std::string& text);
Item parse_update(const std::string& text, const std::string& current);
std::string fetch_text(Http& http, const std::string& url);
std::string read_text(const std::string& path);
void atomic_write(const std::string& path, const std::string& text);
uint64_t file_size(const std::string& path);
void make_directory(const std::string& path);
enum class Outcome { complete, paused };
struct Progress {
    uint64_t done = 0, total = 0;
    bool restarted = false, verifying = false;
};
Outcome transfer(Http& http, const Item& item, const std::string& directory,
                 const std::function<bool()>& stop,
                 const std::function<void(Progress)>& progress);
std::string package_path(const Item& item, const std::string& directory);
std::string lower(std::string value);
std::string trim(const std::string& value);
}
