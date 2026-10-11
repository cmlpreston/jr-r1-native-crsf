#pragma once
#include <cstring>
// One DNS label; the UI supplies the .local suffix separately.
inline bool normalizeHostname(const char *input, char (&output)[33]) {
    if(!input)return false;
    const size_t n=strlen(input);
    if(n<1||n>32||input[0]=='-'||input[n-1]=='-')return false;
    for(size_t i=0;i<n;++i){
        char c=input[i];if(c>='A'&&c<='Z')c+= 'a'-'A';
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
        output[i]=c;
    }
    output[n]=0;return true;
}
