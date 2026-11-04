#include "core.hpp"
#include "library.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace voya;
template<class F> void rejects(F action) {
    bool failed=false;try {action();} catch(const Error&) {failed=true;} assert(failed);
}
struct MetadataResponse : Response {
    std::string text="{\"schema\":1,\"title_id\":\"VOYA00001\",\"version\":\"" VOYA_VERSION "\"}";
    MetadataResponse() {status=200;}
    size_t read(void* buffer,size_t capacity) override {
        auto count=std::min(capacity,text.size());memcpy(buffer,text.data(),count);text.erase(0,count);return count;
    }
};
struct LocalTestHttp : Http {
    std::unique_ptr<Http> network=make_http();
    std::unique_ptr<Response> open(const std::string& url,uint64_t offset,const std::string& validator) override {
        if(url.find("/update.json")!=std::string::npos) return std::make_unique<MetadataResponse>();
        return network->open(url,offset,validator);
    }
};
int main(int argc,char** argv) {
    try {
        if(argc==1) {
            assert(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
            assert(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
            assert(sha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
            assert(valid_url("https://example.com/file.pkg?a=b"));
            assert(valid_url("http://192.168.1.10:8000/file.pkg"));
            assert(!valid_url("http://example.com/file",true));
            assert(!valid_url("https://user:password@example.com/file"));
            assert(!valid_url("file:///etc/passwd"));
            assert(!valid_url("https://example.com/\r\nX-Evil: yes"));
            assert(!valid_url("https://example.com/#fragment"));
            assert(newer_version("0.10.0","0.9.9"));
            assert(!newer_version("0.1.0","0.1.0"));
            assert(!newer_version("0.0.9","0.1.0"));
            rejects([]{newer_version("1.2","0.1.0");});
            rejects([]{parse_catalog("{\"schema\":1,\"packages\":[{\"name\":\"x\",\"url\":\"https://example.com\",\"size\":-1}]}");});
            rejects([]{parse_catalog("{\"schema\":2,\"packages\":[]}");});
            rejects([]{parse_update("{\"schema\":1,\"title_id\":\"VOYA00001\",\"version\":\"0.2.0\",\"name\":\"update\",\"url\":\"https://evil.example/payload.pkg\",\"sha256\":\""+std::string(64,'a')+"\",\"size\":20}","0.1.0");});
            std::string manifest="{\"schema\":1,\"title_id\":\"VOYA00001\",\"version\":\"0.2.0\",\"name\":\"update\",\"url\":\"https://github.com/Abodysoma2021/VoyaPS4/releases/download/v0.2.0/VoyaPS4.pkg\",\"sha256\":\""+std::string(64,'a')+"\",\"size\":20}";
            assert(parse_update(manifest,"0.1.0").update);
            assert(parse_update(manifest,"0.2.0").url.empty());
            assert(parse_update(manifest,"1.0.0").url.empty());
            auto no_hash=manifest;no_hash.replace(no_hash.find(std::string(64,'a')),64,"");
            rejects([&]{parse_update(no_hash,"0.1.0");});
            std::cout<<"Core vectors and metadata policy passed\n";return 0;
        }
        if(std::string(argv[1])=="hash") {std::cout<<file_sha256(argv[2])<<'\n';return 0;}
        if(std::string(argv[1])=="recover") {
            Library library(argv[2],{},nullptr,false);
            auto snapshot=library.snapshot();
            for(auto& j:snapshot.jobs) std::cout<<state_label(j.state)<<'\n';
            std::cout<<snapshot.notice<<'\n';return 0;
        }
        if(argc<7) return 64;
        Item item;item.name="Test package";item.url=argv[2];item.size=std::stoull(argv[3]);item.sha256=argv[4];
        if(std::string(argv[1])=="queue-download") {
            Library library(argv[5],{},std::make_unique<LocalTestHttp>());
            library.enqueue(item);
            auto until=std::chrono::steady_clock::now()+std::chrono::seconds(12);
            while(std::chrono::steady_clock::now()<until) {
                auto job=library.snapshot().jobs.at(0);
                if(job.state==State::ready) {std::cout<<"complete attempts="<<job.attempts<<'\n';return 0;}
                if(job.state==State::failed) throw Error(job.message);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            throw Error("Queue test timed out");
        }
        auto http=make_http();
        uint64_t pause_after=std::stoull(argv[6]);bool paused=false;
        auto result=transfer(*http,item,argv[5],[&]{return paused;},[&](Progress p){
            if(pause_after && p.done>=pause_after) paused=true;
        });
        std::cout<<(result==Outcome::complete?"complete":"paused")<<'\n';return 0;
    } catch(const Error& e) {std::cerr<<e.what()<<'\n';return e.retryable?75:1;}
      catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
