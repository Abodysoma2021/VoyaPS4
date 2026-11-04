#include "core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <exception>
#include <pthread.h>

namespace {
std::string payload() {
    std::string data(256*1024,'x');
    data.replace(0,4,"\x7f" "CNT",4);return data;
}
struct LocalResponse : voya::Response {
    std::string data=payload();size_t position=0;
    LocalResponse() {status=200;headers["content-length"]=std::to_string(data.size());}
    size_t read(void* buffer,size_t capacity) override {
        auto count=std::min(capacity,data.size()-position);
        memcpy(buffer,data.data()+position,count);position+=count;return count;
    }
};
struct LocalHttp : voya::Http {
    std::unique_ptr<voya::Response> open(const std::string&,uint64_t,const std::string&) override {
        return std::make_unique<LocalResponse>();
    }
};
struct Work {std::string directory;std::exception_ptr failure;};
void* download(void* context) {
    auto& work=*static_cast<Work*>(context);
    try {
        LocalHttp http;voya::Item item;
        item.name="Stack regression";item.url="https://example.test/homebrew.pkg";
        auto body=payload();item.size=body.size();item.sha256=voya::sha256(body);
        auto result=voya::transfer(http,item,work.directory,[]{return false;},[](voya::Progress){});
        assert(result==voya::Outcome::complete);
        assert(voya::file_sha256(voya::package_path(item,work.directory))==item.sha256);
    } catch(...) {work.failure=std::current_exception();}
    return nullptr;
}
}
int main(int argc,char** argv) {
    assert(argc==2);Work work{argv[1],{}};
    pthread_attr_t attributes;assert(pthread_attr_init(&attributes)==0);
    assert(pthread_attr_setstacksize(&attributes,64*1024)==0);
    pthread_t thread;assert(pthread_create(&thread,&attributes,download,&work)==0);
    pthread_attr_destroy(&attributes);assert(pthread_join(thread,nullptr)==0);
    if(work.failure) std::rethrow_exception(work.failure);
    puts("Download and file hashing complete on a 64 KiB worker stack");
}
