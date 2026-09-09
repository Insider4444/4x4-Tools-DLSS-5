#pragma once
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
namespace adobe_dlss5::updates {
struct Version {
    std::array<unsigned,3> parts{};
    static bool parse(std::string_view text,Version& result) {
        if(!text.empty()&&text.front()=='v')text.remove_prefix(1);
        if(text.empty()||text.size()>17)return false;
        Version parsed;size_t start=0;unsigned count=0;
        while(start<text.size()&&count<3) {
            const auto end=text.find('.',start);const auto part=text.substr(start,end==text.npos?text.size()-start:end-start);
            if(part.empty()||part.size()>5||(part.size()>1&&part[0]=='0'))return false;
            unsigned value=0;for(char c:part){if(c<'0'||c>'9')return false;value=value*10+unsigned(c-'0');}
            if(value>65535)return false;parsed.parts[count++]=value;
            if(end==text.npos){start=text.size();break;}start=end+1;if(start==text.size())return false;
        }
        if(start!=text.size()||count<2)return false;result=parsed;return true;
    }
    std::string text()const{return std::to_string(parts[0])+'.'+std::to_string(parts[1])+'.'+std::to_string(parts[2]);}
    bool operator>(const Version& other)const{return parts>other.parts;}
};
// A bounded JSON reader, rather than searching release-note strings for "tag_name".
class ReleaseJson {
    std::string_view input;size_t cursor=0;
    void white(){while(cursor<input.size()&&std::isspace(static_cast<unsigned char>(input[cursor])))++cursor;}
    bool take(char c){white();if(cursor<input.size()&&input[cursor]==c){++cursor;return true;}return false;}
    void expect(char c){if(!take(c))throw std::runtime_error("Invalid release JSON");}
    std::string string(){expect('"');std::string value;
        while(cursor<input.size()){char c=input[cursor++];if(c=='"')return value;
            if(static_cast<unsigned char>(c)<32)break;
            if(c=='\\'){if(cursor==input.size())break;c=input[cursor++];
                if(c=='u'){for(int i=0;i<4;++i){if(cursor==input.size()||!std::isxdigit(static_cast<unsigned char>(input[cursor++])))throw std::runtime_error("Invalid unicode escape");}value+='?';continue;}
                if(std::string_view("\"\\/bfnrt").find(c)==std::string_view::npos)break;
            }value+=c;
        }throw std::runtime_error("Unterminated JSON string");
    }
    bool boolean(){white();if(input.substr(cursor,4)=="true"){cursor+=4;return true;}if(input.substr(cursor,5)=="false"){cursor+=5;return false;}throw std::runtime_error("Invalid JSON boolean");}
    void skip(unsigned depth=0){if(depth>32)throw std::runtime_error("Release nesting too deep");white();
        if(cursor>=input.size())throw std::runtime_error("Missing value");
        if(input[cursor]=='"'){string();return;}
        if(take('{')){if(take('}'))return;do{string();expect(':');skip(depth+1);}while(take(','));expect('}');return;}
        if(take('[')){if(take(']'))return;do{skip(depth+1);}while(take(','));expect(']');return;}
        const auto begin=cursor;while(cursor<input.size()&&std::string_view(",]} \t\r\n").find(input[cursor])==std::string_view::npos)++cursor;
        const auto token=input.substr(begin,cursor-begin);
        if(token=="true"||token=="false"||token=="null")return;
        if(token.empty())throw std::runtime_error("Empty JSON token");
        size_t i=0; if(token[i]=='-')++i;
        const auto digits=[&](){const auto first=i;while(i<token.size()&&token[i]>='0'&&token[i]<='9')++i;return i>first;};
        if(i==token.size())throw std::runtime_error("Invalid JSON number");
        if(token[i]=='0')++i;else if(!digits())throw std::runtime_error("Invalid JSON number");
        if(i<token.size()&&token[i]=='.'){++i;if(!digits())throw std::runtime_error("Invalid JSON fraction");}
        if(i<token.size()&&(token[i]=='e'||token[i]=='E')){++i;if(i<token.size()&&(token[i]=='+'||token[i]=='-'))++i;if(!digits())throw std::runtime_error("Invalid JSON exponent");}
        if(i!=token.size())throw std::runtime_error("Invalid JSON number");
    }
public:
    explicit ReleaseJson(std::string_view text):input(text){}
    bool stableVersion(Version& version){if(input.size()>131072)return false;
        try{expect('{');bool tagSeen=false,draftSeen=false,preSeen=false,draft=true,pre=true;std::string tag;
            if(take('}'))return false;
            do{const auto key=string();expect(':');
                if(key=="tag_name"){if(tagSeen)return false;tagSeen=true;tag=string();}
                else if(key=="draft"){if(draftSeen)return false;draftSeen=true;draft=boolean();}
                else if(key=="prerelease"){if(preSeen)return false;preSeen=true;pre=boolean();}
                else skip();
            }while(take(','));expect('}');white();
            return cursor==input.size()&&tagSeen&&draftSeen&&preSeen&&!draft&&!pre&&Version::parse(tag,version);
        }catch(...){return false;}
    }
};
}
