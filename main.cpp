#include "pic_test.h"
#include <iostream>
#include "deepstream_test.h"

int main(int argc, char *argv[])
{
    //**************测试socket功能**************
    // std::thread worker(socket_func);
    // worker.detach();
    //**************测试socket功能**************
    
    //**************测试deepstream的功能**************
    deepstream_func();
    //**************测试deepstream的功能**************

    //**************测试rtmp功能**************
    // rtmp_func();
    //**************测试rtmp功能**************

}

