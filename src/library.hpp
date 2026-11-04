#pragma once
#include "core.hpp"
#include <mutex>
#include <thread>

#ifndef VOYA_VERSION
#define VOYA_VERSION "0.1.0"
#endif
namespace voya {
enum class State { queued, downloading, retrying, paused, ready, failed, cancelled };
const char* state_label(State state);
struct Job {
    Item item;
    State state=State::queued;
    uint64_t done=0,total=0;
    unsigned attempts=0;
    int64_t next_attempt=0;
    std::string message;
    double bytes_per_second=0;
};
struct Snapshot {
    std::vector<Item> catalog;
    std::vector<Job> jobs;
    Item update;
    std::string notice="Ready. Add a direct package URL or choose homebrew.";
    std::string catalog_url;
};
class Library {
    std::string directory;
    mutable std::mutex mutex;
    Snapshot data;
    std::unique_ptr<Http> http;
    std::thread worker;
    std::atomic<bool> stopping{false},interrupt{false};
    std::string active;
    bool check_requested=true,refresh_requested=false;
    void save_locked();
    void run();
    void load();
public:
    Library(std::string directory,std::vector<Item> bundled,std::unique_ptr<Http> http,bool start=true);
    ~Library();
    Snapshot snapshot() const;
    void enqueue(const Item& item);
    void toggle(const std::string& id);
    void cancel(const std::string& id);
    void restart(const std::string& id);
    void check_update();
    void set_catalog(const std::string& url);
    void notice(const std::string& message);
    std::string path(const Item& item) const { return package_path(item,directory); }
};
}
