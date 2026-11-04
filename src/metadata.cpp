#include "core.hpp"
#include "json.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace voya {
using json = nlohmann::json;
std::string lower(std::string value) {
    for(auto& c:value) c=char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
std::string trim(const std::string& value) {
    auto first=value.find_first_not_of(" \r\n\t");
    if(first==std::string::npos) return {};
    return value.substr(first,value.find_last_not_of(" \r\n\t")-first+1);
}
std::string Item::id() const { return voya::sha256(url+"\n"+sha256).substr(0,24); }
std::string Response::header(const std::string& name) const {
    auto it=headers.find(lower(name));
    return it==headers.end()?"":it->second;
}
bool valid_url(const std::string& url, bool https_only) {
    size_t start=url.rfind("https://",0)==0?8:(!https_only&&url.rfind("http://",0)==0?7:0);
    if(!start || url.size()>2048 || url.size()<=start) return false;
    for(unsigned char c:url) if(c<=32 || c>=127 || c=='\\') return false;
    auto end=url.find_first_of("/?#",start);
    auto authority=url.substr(start,end-start);
    return !authority.empty() && authority.find('@')==std::string::npos &&
           url.find('#')==std::string::npos;
}
namespace {
std::array<int,3> version_parts(const std::string& text) {
    std::array<int,3> parts{};
    size_t pos=0;
    for(int i=0;i<3;++i) {
        if(pos>=text.size() || !isdigit(static_cast<unsigned char>(text[pos])))
            throw Error("Invalid version");
        int value=0;
        while(pos<text.size() && isdigit(static_cast<unsigned char>(text[pos]))) {
            value=value*10+text[pos++]-'0';
            if(value>9999) throw Error("Invalid version");
        }
        parts[i]=value;
        if(i<2 && (pos==text.size() || text[pos++]!='.')) throw Error("Invalid version");
    }
    if(pos!=text.size()) throw Error("Invalid version");
    return parts;
}
Item decode_item(const json& data) {
    Item item;
    item.name=data.at("name").get<std::string>();
    item.url=data.at("url").get<std::string>();
    item.sha256=lower(data.value("sha256",std::string{}));
    if(data.contains("size")) {
        if(!data["size"].is_number_unsigned()) throw Error("Package size must be a nonnegative integer");
        item.size=data["size"].get<uint64_t>();
    }
    item.description=data.value("description",std::string{});
    item.source=data.value("source",std::string{});
    validate_item(item);
    return item;
}
}
bool newer_version(const std::string& candidate, const std::string& current) {
    return version_parts(candidate)>version_parts(current);
}
void validate_item(const Item& item) {
    if(item.name.empty() || item.name.size()>100 || item.description.size()>400 ||
       item.source.size()>2048 || !valid_url(item.url) || item.size>max_package_size)
        throw Error("Invalid package metadata or URL");
    for(unsigned char c:item.name) if(c<32 || c==127) throw Error("Invalid package name");
    if(!item.sha256.empty() && (item.sha256.size()!=64 ||
       item.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos))
        throw Error("Invalid SHA-256 checksum");
    if(item.update && (!valid_url(item.url,true) || item.sha256.empty() || !item.size))
        throw Error("Updates require HTTPS, size and SHA-256");
}
std::vector<Item> parse_catalog(const std::string& text) {
    try {
        auto root=json::parse(text);
        if(root.at("schema")!=1 || !root.at("packages").is_array() || root["packages"].size()>200)
            throw Error("Unsupported catalog format");
        std::vector<Item> items;
        for(const auto& entry:root["packages"]) items.push_back(decode_item(entry));
        return items;
    } catch(const json::exception&) { throw Error("Catalog JSON is invalid"); }
}
Item parse_update(const std::string& text, const std::string& current) {
    try {
        auto root=json::parse(text);
        if(root.at("schema")!=1 || root.at("title_id")!="VOYA00001") throw Error("Wrong update application");
        auto version=root.at("version").get<std::string>();
        if(!newer_version(version,current)) return {};
        auto item=decode_item(root);
        item.update=true;
        validate_item(item);
        // Limit updater to immutable artifacts in this project's release namespace.
        const std::string prefix="https://github.com/Abodysoma2021/VoyaPS4/releases/download/v"+version+"/";
        if(item.url.rfind(prefix,0)!=0 || item.url.substr(prefix.size())!="VoyaPS4.pkg")
            throw Error("Update URL is outside the official release");
        return item;
    } catch(const json::exception&) { throw Error("Update manifest is invalid"); }
}
uint64_t file_size(const std::string& path) {
    struct stat st{};
    if(lstat(path.c_str(),&st)<0) {
        if(errno==ENOENT) return 0;
        throw Error("Cannot inspect local file");
    }
    if(!S_ISREG(st.st_mode) || st.st_size<0) throw Error("Storage path is not a regular file");
    return static_cast<uint64_t>(st.st_size);
}
std::string read_text(const std::string& path) {
    auto length=file_size(path);
    if(length>1024*1024) throw Error("Metadata file is too large");
    std::unique_ptr<FILE,decltype(&fclose)> file(fopen(path.c_str(),"rb"),fclose);
    if(!file) throw Error("Cannot open metadata file");
    std::string text(length,'\0');
    if(fread(&text[0],1,length,file.get())!=length) throw Error("Cannot read metadata file");
    return text;
}
void atomic_write(const std::string& path, const std::string& text) {
    std::string temporary=path+".tmp";
    int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if(fd<0) throw Error("Cannot save state: check free storage");
    size_t done=0;
    while(done<text.size()) {
        ssize_t count=write(fd,text.data()+done,text.size()-done);
        if(count<=0) { close(fd); throw Error("Cannot save state: storage write failed"); }
        done+=static_cast<size_t>(count);
    }
    int synced=fsync(fd);
    int closed=close(fd);
    if(synced || closed || rename(temporary.c_str(),path.c_str())) throw Error("Cannot commit saved state");
    auto slash=path.find_last_of('/');
    int dir=open((slash==std::string::npos?".":path.substr(0,slash)).c_str(),O_RDONLY|O_DIRECTORY);
    if(dir>=0) { fsync(dir); close(dir); }
}
void make_directory(const std::string& path) {
    if(mkdir(path.c_str(),0700)<0 && errno!=EEXIST) throw Error("Cannot create download folder");
    struct stat st{};
    if(lstat(path.c_str(),&st) || !S_ISDIR(st.st_mode)) throw Error("Download folder is not a directory");
}
std::string fetch_text(Http& http, const std::string& url) {
    if(!valid_url(url,true)) throw Error("Catalog and update URLs require HTTPS");
    auto response=http.open(url,0,{});
    if(response->status!=200) throw Error("Metadata server returned HTTP "+std::to_string(response->status));
    std::string text;
    char buffer[8192];
    while(auto count=response->read(buffer,sizeof(buffer))) {
        if(text.size()+count>1024*1024) throw Error("Remote metadata is too large");
        text.append(buffer,count);
    }
    return text;
}
}
