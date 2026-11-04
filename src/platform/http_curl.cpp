#include "core.hpp"
#include <algorithm>
#include <cstring>
#include <curl/curl.h>

namespace voya {
namespace {
class CurlResponse final : public Response {
    CURL* easy=nullptr;
    CURLM* multi=nullptr;
    curl_slist* request_headers=nullptr;
    std::vector<uint8_t> pending;
    bool headers_ready=false,paused=false,finished=false;
    CURLcode result=CURLE_OK;
    static size_t header_cb(char* bytes,size_t size,size_t count,void* context) {
        auto& self=*static_cast<CurlResponse*>(context);
        std::string line(bytes,size*count);
        if(line.rfind("HTTP/",0)==0) {
            self.headers.clear();
            auto space=line.find(' ');
            self.status=space==std::string::npos?0:std::atoi(line.c_str()+space+1);
        } else if(line=="\r\n") self.headers_ready=self.status>=200;
        else {
            auto colon=line.find(':');
            if(colon!=std::string::npos) self.headers[lower(line.substr(0,colon))]=trim(line.substr(colon+1));
        }
        return size*count;
    }
    static size_t write_cb(char* bytes,size_t size,size_t count,void* context) {
        auto& self=*static_cast<CurlResponse*>(context);
        size_t length=size*count;
        if(self.pending.size()+length>256*1024) {
            self.paused=true;
            return CURL_WRITEFUNC_PAUSE;
        }
        self.pending.insert(self.pending.end(),bytes,bytes+length);
        return length;
    }
    void step() {
        int running=0;
        if(curl_multi_perform(multi,&running)!=CURLM_OK) throw Error("Network transport failed",true);
        int remaining=0;
        while(auto message=curl_multi_info_read(multi,&remaining)) {
            if(message->msg==CURLMSG_DONE) { finished=true; result=message->data.result; }
        }
        if(!finished) {
            int count=0;
            if(curl_multi_wait(multi,nullptr,0,100,&count)!=CURLM_OK) throw Error("Network wait failed",true);
        }
    }
    void check_error() {
        if(finished && result!=CURLE_OK) {
            bool retry=result==CURLE_OPERATION_TIMEDOUT || result==CURLE_COULDNT_RESOLVE_HOST ||
                       result==CURLE_COULDNT_CONNECT || result==CURLE_RECV_ERROR || result==CURLE_PARTIAL_FILE;
            throw Error(std::string("Network: ")+curl_easy_strerror(result),retry);
        }
    }
public:
    CurlResponse(const std::string& url,uint64_t offset,const std::string& validator) {
        easy=curl_easy_init(); multi=curl_multi_init();
        if(!easy || !multi) throw Error("Cannot initialize network transport");
        curl_easy_setopt(easy,CURLOPT_URL,url.c_str());
        curl_easy_setopt(easy,CURLOPT_USERAGENT,"VoyaPS4/0.1");
        curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,0L);
        curl_easy_setopt(easy,CURLOPT_PROTOCOLS_STR,"http,https");
        curl_easy_setopt(easy,CURLOPT_CONNECTTIMEOUT,15L);
        curl_easy_setopt(easy,CURLOPT_LOW_SPEED_LIMIT,1L);
        curl_easy_setopt(easy,CURLOPT_LOW_SPEED_TIME,30L);
        curl_easy_setopt(easy,CURLOPT_NOSIGNAL,1L);
        curl_easy_setopt(easy,CURLOPT_HEADERFUNCTION,header_cb);
        curl_easy_setopt(easy,CURLOPT_HEADERDATA,this);
        curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,write_cb);
        curl_easy_setopt(easy,CURLOPT_WRITEDATA,this);
        request_headers=curl_slist_append(request_headers,"Accept-Encoding: identity");
        if(offset) {
            request_headers=curl_slist_append(request_headers,("Range: bytes="+std::to_string(offset)+"-").c_str());
            if(!validator.empty()) request_headers=curl_slist_append(request_headers,("If-Range: "+validator).c_str());
        }
        curl_easy_setopt(easy,CURLOPT_HTTPHEADER,request_headers);
        curl_multi_add_handle(multi,easy);
    }
    void receive_headers() {
        while(!headers_ready && !finished) step();
        check_error();
        if(!headers_ready) throw Error("Server did not send HTTP response headers",true);
    }
    ~CurlResponse() override {
        if(multi && easy) curl_multi_remove_handle(multi,easy);
        if(easy) curl_easy_cleanup(easy);
        if(multi) curl_multi_cleanup(multi);
        curl_slist_free_all(request_headers);
    }
    size_t read(void* destination,size_t capacity) override {
        if(paused && pending.empty()) {
            paused=false;
            curl_easy_pause(easy,CURLPAUSE_CONT);
        }
        while(pending.empty() && !finished) step();
        if(pending.empty()) { check_error(); return 0; }
        auto count=std::min(capacity,pending.size());
        memcpy(destination,pending.data(),count);
        pending.erase(pending.begin(),pending.begin()+count);
        return count;
    }
};
class CurlHttp final : public Http {
public:
    CurlHttp() { if(curl_global_init(CURL_GLOBAL_DEFAULT)) throw Error("Cannot initialize libcurl"); }
    ~CurlHttp() override { curl_global_cleanup(); }
    std::unique_ptr<Response> open(const std::string& initial,uint64_t offset,const std::string& validator) override {
        std::string url=initial;
        for(int redirects=0;redirects<=5;++redirects) {
            if(!valid_url(url,initial.rfind("https://",0)==0)) throw Error("Unsafe redirect URL");
            auto response=std::make_unique<CurlResponse>(url,offset,validator);
            response->receive_headers();
            if(response->status<300 || response->status>399) return response;
            auto location=response->header("location");
            if(location.rfind("/",0)==0 && location.rfind("//",0)!=0) {
                auto slash=url.find('/',url.find("://")+3);
                location=url.substr(0,slash)+location;
            }
            url=location;
        }
        throw Error("Too many HTTP redirects");
    }
};
}
std::unique_ptr<Http> make_http() { return std::make_unique<CurlHttp>(); }
}
