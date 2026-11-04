#include "core.hpp"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <cstdio>

// OpenOrbis 0.5.4 has placeholder void() declarations for these two exports.
// Use the typed ABI without modifying SDK headers. Hardware verification is required.
extern "C" int32_t voya_http_recv_timeout(int32_t,uint32_t) __asm__("sceHttpSetRecvTimeOut");
extern "C" int32_t voya_http_auto_redirect(int32_t,int32_t) __asm__("sceHttpSetAutoRedirect");

namespace voya {
namespace {
void check(int32_t code,const char* operation) {
    if(code>=0) return;
    char text[150];snprintf(text,sizeof(text),"%s (0x%08x)",operation,unsigned(code));
    throw Error(text,true);
}
class Ps4Response final : public Response {
    int32_t tmpl=-1,connection=-1,request=-1;
public:
    void start(int32_t context,const std::string& url,uint64_t offset,const std::string& validator) {
        tmpl=sceHttpCreateTemplate(context,"VoyaPS4/" VOYA_VERSION,ORBIS_HTTP_VERSION_1_1,0);
        check(tmpl,"Cannot create HTTP context");
        // Preserve the platform's certificate and hostname verification defaults.
        // Never install the SDK example's skipSslCallback or disable HTTPS options.
        check(voya_http_auto_redirect(tmpl,0),"Cannot configure redirects");
        check(sceHttpSetConnectTimeOut(tmpl,15*1000*1000),"Cannot set connect timeout");
        check(sceHttpSetResolveTimeOut(tmpl,10*1000*1000),"Cannot set DNS timeout");
        check(voya_http_recv_timeout(tmpl,30*1000*1000),"Cannot set receive timeout");
        connection=sceHttpCreateConnectionWithURL(tmpl,url.c_str(),false);
        check(connection,"Cannot create HTTP connection");
        request=sceHttpCreateRequestWithURL(connection,ORBIS_METHOD_GET,url.c_str(),0);
        check(request,"Cannot create HTTP request");
        check(sceHttpAddRequestHeader(request,"Accept-Encoding","identity",0),"Cannot set request header");
        if(offset) {
            auto range="bytes="+std::to_string(offset)+"-";
            check(sceHttpAddRequestHeader(request,"Range",range.c_str(),0),"Cannot set download range");
            if(!validator.empty())
                check(sceHttpAddRequestHeader(request,"If-Range",validator.c_str(),0),"Cannot set range validator");
        }
        check(sceHttpSendRequest(request,nullptr,0),"Connection failed; check network and console clock");
        check(sceHttpGetStatusCode(request,&status),"Cannot read HTTP status");
        char* raw=nullptr;size_t size=0;
        check(sceHttpGetAllResponseHeaders(request,&raw,&size),"Cannot read response headers");
        if(!raw || size>64*1024) throw Error("HTTP response headers are too large");
        std::string all(raw,size);
        size_t start=0;
        while(start<all.size()) {
            auto end=all.find('\n',start);
            auto line=all.substr(start,end==std::string::npos?end:end-start);
            auto colon=line.find(':');
            if(colon!=std::string::npos) headers[lower(line.substr(0,colon))]=trim(line.substr(colon+1));
            if(end==std::string::npos) break;
            start=end+1;
        }
    }
    ~Ps4Response() override {
        if(request>=0) sceHttpDeleteRequest(request);
        if(connection>=0) sceHttpDeleteConnection(connection);
        if(tmpl>=0) sceHttpDeleteTemplate(tmpl);
    }
    size_t read(void* buffer,size_t size) override {
        int32_t count=sceHttpReadData(request,buffer,static_cast<uint32_t>(size));
        check(count,"Network interrupted; download will resume");
        return static_cast<size_t>(count);
    }
};
class Ps4Http final : public Http {
    int32_t pool=-1,ssl=-1,context=-1;
public:
    void init() {
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET),"Cannot load network module");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL),"Cannot load SSL module");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP),"Cannot load HTTP module");
        // The network library may already be initialized by the loader.
        sceNetInit();
        if(pool<0) {pool=sceNetPoolCreate("voyaps4",4*1024*1024,0);check(pool,"Cannot allocate network pool");}
        if(ssl<0) {ssl=sceSslInit(4*1024*1024);check(ssl,"Cannot initialize SSL");}
        context=sceHttpInit(pool,ssl,4*1024*1024);check(context,"Cannot initialize HTTP");
    }
    ~Ps4Http() override {
        if(context>=0) sceHttpTerm(context);
        // SDK 0.5.4 declares sceSslTerm as a placeholder; process teardown owns SSL.
        if(pool>=0) sceNetPoolDestroy(pool);
    }
    std::unique_ptr<Response> open(const std::string& initial,uint64_t offset,const std::string& validator) override {
        // Keep the controller UI and saved queue available if networking cannot initialize.
        if(context<0) init();
        std::string url=initial;
        for(int redirects=0;redirects<=5;++redirects) {
            if(!valid_url(url,initial.rfind("https://",0)==0)) throw Error("Unsafe redirect URL");
            auto response=std::make_unique<Ps4Response>();
            response->start(context,url,offset,validator);
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
std::unique_ptr<Http> make_http() {
    return std::make_unique<Ps4Http>();
}
}
