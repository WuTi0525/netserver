#pragma once
#include <string>
#include <cstring>
#include <iostream>

class Buffer
{
private:
    std::string buf_;                // 用于存放数据。
    const uint16_t sep_;         // 报文的分隔符：0-无分隔符(固定长度、视频会议)；1-四字节的报头；2-"\r\n\r\n"分隔符（http协议）。

public:
    Buffer(uint16_t sep=1);
    ~Buffer();

    void append(const char *data,size_t size);                 // 把数据追加到buf_中。
    void appendwithsep(const char *data,size_t size);    // 把数据追加到buf_中，附加报文分隔符。
    void erase(size_t pos,size_t nn);                                 // 从buf_的pos开始，删除nn个字节，pos从0开始。
    size_t size();                                                                // 返回buf_的大小。
    const char *data();                                                      // 返回buf_的首地址。
    void clear();                                                                // 清空buf_。
    bool pickmessage(std::string &ss);                           // 从buf_中拆分出一个报文，存放在ss中，如果buf_中没有报文，返回false。
};