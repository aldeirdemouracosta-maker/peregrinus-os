#include <stddef.h>

extern "C" void* memset(void* dst,int value,size_t n){
    volatile unsigned char* d=static_cast<volatile unsigned char*>(dst);
    const unsigned char v=static_cast<unsigned char>(value);
    for(size_t i=0;i<n;++i)d[i]=v;
    return dst;
}

extern "C" void* memcpy(void* dst,const void* src,size_t n){
    volatile unsigned char* d=static_cast<volatile unsigned char*>(dst);
    const volatile unsigned char* s=static_cast<const volatile unsigned char*>(src);
    for(size_t i=0;i<n;++i)d[i]=s[i];
    return dst;
}

extern "C" void* memmove(void* dst,const void* src,size_t n){
    volatile unsigned char* d=static_cast<volatile unsigned char*>(dst);
    const volatile unsigned char* s=static_cast<const volatile unsigned char*>(src);
    if(d<s){for(size_t i=0;i<n;++i)d[i]=s[i];}
    else if(d>s){for(size_t i=n;i>0;--i)d[i-1]=s[i-1];}
    return dst;
}
