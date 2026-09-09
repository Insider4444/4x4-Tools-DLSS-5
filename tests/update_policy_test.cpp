#include "update_policy.h"
#include <iostream>
#include <stdexcept>
using namespace adobe_dlss5::updates;
static void require(bool value){if(!value)throw std::runtime_error("Update policy test failed");}
int main(){try{
    Version current,newer,parsed;require(Version::parse("v1.0",current)&&current.text()=="1.0.0");
    require(Version::parse("1.10.0",newer)&&newer>current);require(!Version::parse("1.01.0",parsed));
    for(const char* invalid:{"","v1","1.2.","1.2.3.4","1.2.3-beta","65536.0.0","https://evil.example","1.-2.0","1.2.3&calc"})require(!Version::parse(invalid,parsed));
    require(ReleaseJson(R"({"tag_name":"v1.2.0","draft":false,"prerelease":false,"body":"Ignore \"tag_name\":\"v9.9.9\""})").stableVersion(parsed)&&parsed.text()=="1.2.0");
    require(ReleaseJson(R"({"body":{"tag_name":"v99.0.0"},"assets":[{"name":"x"}],"tag_name":"v1.1.0","draft":false,"prerelease":false})").stableVersion(parsed)&&parsed.text()=="1.1.0");
    for(const char* invalid:{R"({"tag_name":"v2.0.0","draft":true,"prerelease":false})",R"({"tag_name":"v2.0.0","draft":false,"prerelease":true})",R"({"tag_name":"v2.0.0"})",R"({"tag_name":"v2.0.0","tag_name":"v3.0.0","draft":false,"prerelease":false})",R"({"tag_name":"v2.0.0","draft":false,"prerelease":false}garbage)"})require(!ReleaseJson(invalid).stableVersion(parsed));
    require(!ReleaseJson(std::string(131073,' ')).stableVersion(parsed));
    std::cout<<"PASS semantic version ordering, legacy v1.0 tags, stable-only updates, malformed/duplicate/nested/oversized JSON rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
