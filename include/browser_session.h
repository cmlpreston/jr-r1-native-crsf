#pragma once
#include <cstring>
// Match a complete cookie field, never a substring or a partial session ID.
inline bool browserSessionMatches(const char *cookies,const char *session) {
    if(!cookies||!session||!*session)return false;
    const char name[]="R1Session=";
    while(*cookies) {
        const char *end=std::strchr(cookies,';');
        if(!end)end=cookies+std::strlen(cookies);
        const char *start=cookies,*last=end;
        while(start<last&&(*start==' '||*start=='\t'))++start;
        while(last>start&&(last[-1]==' '||last[-1]=='\t'))--last;
        const size_t n=sizeof(name)-1,len=std::strlen(session);
        if(size_t(last-start)==n+len&&std::memcmp(start,name,n)==0&&std::memcmp(start+n,session,len)==0)return true;
        cookies=*end?end+1:end;
    }
    return false;
}
