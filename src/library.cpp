#include "library.hpp"
#include "json.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sys/stat.h>

namespace voya {
using json=nlohmann::json;
namespace {
int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
json encode_item(const Item& i) {
    return {{"name",i.name},{"url",i.url},{"sha256",i.sha256},{"size",i.size},
            {"description",i.description},{"source",i.source}};
}
}
const char* state_label(State state) {
    switch(state) {
        case State::queued:return "Queued";
        case State::downloading:return "Downloading";
        case State::retrying:return "Reconnecting";
        case State::paused:return "Paused";
        case State::ready:return "Ready to install";
        case State::failed:return "Needs attention";
        case State::cancelled:return "Cancelled";
    }
    return "Unknown";
}
Library::Library(std::string path,std::vector<Item> bundled,std::unique_ptr<Http> transport,bool start)
    :directory(std::move(path)),http(std::move(transport)) {
    make_directory(directory);
    data.catalog=std::move(bundled);
    load();
    if(start) worker=std::thread(&Library::run,this);
}
Library::~Library() {
    stopping=true;interrupt=true;
    if(worker.joinable()) worker.join();
}
void Library::load() {
    struct stat st{};
    if(stat((directory+"/queue.json").c_str(),&st)==0) {
        try {
            auto root=json::parse(read_text(directory+"/queue.json"));
            if(root.at("schema")!=1 || !root.at("jobs").is_array() || root["jobs"].size()>100)
                throw Error("Invalid saved queue");
            std::vector<Job> restored;
            for(const auto& entry:root["jobs"]) {
                auto parsed=parse_catalog(json{{"schema",1},{"packages",json::array({entry.at("item")})}}.dump());
                Job j;j.item=parsed.at(0);
                j.item.update=entry.value("update",false);
                validate_item(j.item);
                auto state=entry.value("state",std::string{});
                if(state=="paused") j.state=State::paused;
                else if(state=="cancelled") j.state=State::cancelled;
                else if(state=="failed") j.state=State::failed;
                else if(state=="ready") j.state=State::ready;
                else j.state=State::queued;
                j.message=entry.value("message",std::string{});
                if(j.message.size()>512) j.message.clear();
                j.done=file_size(package_path(j.item,directory)+(j.state==State::ready?"":".part"));
                j.total=j.item.size;
                if(j.state==State::ready && !j.done) j.state=State::queued;
                restored.push_back(j);
            }
            data.jobs=std::move(restored);
        } catch(const std::exception&) {
            // Preserve the original evidence; do not overwrite it with an empty queue.
            auto backup=directory+"/queue.recovery.json";
            if(rename((directory+"/queue.json").c_str(),backup.c_str()))
                throw Error("Saved queue is invalid and could not be preserved");
            data.notice="Queue metadata was invalid. Preserved as queue.recovery.json; partial packages remain.";
        }
    }
    if(stat((directory+"/catalog.json").c_str(),&st)==0) {
        try { data.catalog=parse_catalog(read_text(directory+"/catalog.json")); }
        catch(const Error& e) { data.notice=e.what(); }
    }
    if(stat((directory+"/catalog-url.txt").c_str(),&st)==0) {
        auto url=trim(read_text(directory+"/catalog-url.txt"));
        if(valid_url(url,true)) data.catalog_url=url;
    }
}
void Library::save_locked() {
    json jobs=json::array();
    for(const auto& j:data.jobs) {
        std::string state="queued";
        if(j.state==State::paused) state="paused";
        if(j.state==State::cancelled) state="cancelled";
        if(j.state==State::failed) state="failed";
        if(j.state==State::ready) state="ready";
        jobs.push_back({{"item",encode_item(j.item)},{"state",state},
                        {"update",j.item.update},{"message",j.message}});
    }
    atomic_write(directory+"/queue.json",json{{"schema",1},{"jobs",jobs}}.dump(2));
}
Snapshot Library::snapshot() const { std::lock_guard<std::mutex> lock(mutex);return data; }
void Library::notice(const std::string& message) { std::lock_guard<std::mutex> lock(mutex);data.notice=message; }
void Library::enqueue(const Item& item) {
    validate_item(item);
    std::lock_guard<std::mutex> lock(mutex);
    for(auto& job:data.jobs) if(job.item.id()==item.id()) {
        data.notice="This package is already in Downloads.";return;
    }
    if(data.jobs.size()>=100) throw Error("Queue limit reached (100 packages)");
    Job job;job.item=item;job.total=item.size;data.jobs.push_back(job);
    save_locked();data.notice="Added to Downloads. You can pause at any time.";
}
void Library::toggle(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    for(auto& job:data.jobs) if(job.item.id()==id) {
        if(job.state==State::ready) return;
        if(job.state==State::downloading || job.state==State::retrying || job.state==State::queued) {
            job.state=State::paused;if(active==id) interrupt=true;
        } else {job.state=State::queued;job.attempts=0;job.message.clear();job.next_attempt=0;}
    }
    save_locked();
}
void Library::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    for(auto& job:data.jobs) if(job.item.id()==id && job.state!=State::ready) {
        job.state=State::cancelled;if(active==id) interrupt=true;
        job.message="Partial file kept. Press X to resume.";
    }
    save_locked();
}
void Library::restart(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    if(active==id) throw Error("Pause this download before restarting");
    for(auto& job:data.jobs) if(job.item.id()==id && job.state!=State::ready) {
        // Only rename a generated file in our fixed download directory; preserve bytes for recovery.
        auto path=package_path(job.item,directory)+".part";
        if(file_size(path) && rename(path.c_str(),(path+".previous").c_str()))
            throw Error("Cannot preserve old partial package");
        job.state=State::queued;job.done=0;job.attempts=0;job.next_attempt=0;job.message.clear();
    }
    save_locked();
}
void Library::check_update() { std::lock_guard<std::mutex> lock(mutex);check_requested=true; }
void Library::set_catalog(const std::string& url) {
    if(!valid_url(url,true)) throw Error("Enter a valid HTTPS catalog URL");
    std::lock_guard<std::mutex> lock(mutex);
    atomic_write(directory+"/catalog-url.txt",url);
    data.catalog_url=url;refresh_requested=true;data.notice="Catalog refresh queued.";
}
void Library::run() {
    while(!stopping) {
        Item item;
        bool update=false,refresh=false;
        std::string catalog_url;
        {
            std::lock_guard<std::mutex> lock(mutex);
            update=check_requested;check_requested=false;
            refresh=refresh_requested;refresh_requested=false;catalog_url=data.catalog_url;
            if(!update && !refresh) {
                for(auto& j:data.jobs) if(j.state==State::queued ||
                   (j.state==State::retrying && j.next_attempt<=now_seconds())) {
                    item=j.item;j.state=State::downloading;active=item.id();interrupt=false;break;
                }
            }
        }
        try {
            if(update) {
                auto available=parse_update(fetch_text(*http,
                    "https://github.com/Abodysoma2021/VoyaPS4/releases/latest/download/update.json"),VOYA_VERSION);
                std::lock_guard<std::mutex> lock(mutex);
                data.update=available;
                data.notice=available.url.empty()?"VoyaPS4 is up to date.":"A new VoyaPS4 release is available in Updates.";
            } else if(refresh) {
                auto text=fetch_text(*http,catalog_url);
                auto catalog=parse_catalog(text);
                atomic_write(directory+"/catalog.json",text);
                std::lock_guard<std::mutex> lock(mutex);
                data.catalog=std::move(catalog);data.notice="Catalog refreshed.";
            } else if(!item.url.empty()) {
                auto started=std::chrono::steady_clock::now();
                uint64_t initial=0;bool first=true;
                auto result=transfer(*http,item,directory,[&]{return stopping.load() || interrupt.load();},
                    [&](Progress p) {
                        std::lock_guard<std::mutex> lock(mutex);
                        if(first) {initial=p.done;first=false;}
                        for(auto& j:data.jobs) if(j.item.id()==item.id()) {
                            j.done=p.done;j.total=p.total;
                            double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
                            j.bytes_per_second=seconds>0.2 && p.done>=initial?(p.done-initial)/seconds:0;
                            j.message=p.verifying?"Checking package integrity...":(p.restarted?"Server restarted transfer from the beginning.":"");
                        }
                    });
                std::lock_guard<std::mutex> lock(mutex);
                for(auto& j:data.jobs) if(j.item.id()==item.id()) {
                    if(result==Outcome::complete) {
                        j.state=State::ready;j.bytes_per_second=0;
                        j.message=item.sha256.empty()?"No publisher checksum. Format/length checked; source authenticity unverified.":"SHA-256 verified. Press X to request installation.";
                    } else if(j.state==State::downloading) j.state=State::queued;
                }
                active.clear();save_locked();
            } else std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch(const std::exception& exception) {
            std::lock_guard<std::mutex> lock(mutex);
            if(item.url.empty()) data.notice=std::string(update?"Update check: ":"Catalog: ")+exception.what();
            else {
                const auto* error=dynamic_cast<const Error*>(&exception);
                for(auto& j:data.jobs) if(j.item.id()==item.id()) {
                    if(j.state==State::downloading) {
                        ++j.attempts;j.state=error && error->retryable?State::retrying:State::failed;
                        unsigned delay=std::min(120u,2u<<std::min(j.attempts-1,6u));
                        j.next_attempt=now_seconds()+delay;
                        j.message=exception.what();j.bytes_per_second=0;
                    }
                }
                active.clear();
                try {save_locked();} catch(const Error& e) {data.notice=e.what();stopping=true;}
            }
        }
    }
}
}
