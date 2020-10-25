//
// Created by System Administrator on 2020/10/11.
//

#include "WhoisClient.h"
#include "asio/STUtils.h"

string WhoisClient::whois(const string &domain, const string &whoisServerIp, const u_int16_t &whoisServerPort, const u_int32_t &timeout) {
    uint64_t begin = time::now();
    string logTag = "query " + domain + " from whois server " + whoisServerIp + ":" + to_string(whoisServerPort);
    io_context &context = asio::TLIOContext::getIOContext();
    ip::tcp::endpoint serverEndpoint(ip::make_address_v4(whoisServerIp), whoisServerPort);
    ip::tcp::socket socket(context);
    auto connectFuture = socket.async_connect(serverEndpoint, use_future([&](boost::system::error_code error) {
                                                  if (error) {
                                                      Logger::ERROR << logTag << "connect error!" << error.message() << END;
                                                  }
                                                  return !error;
                                              }));

    char sBuffer[20480];
    context.restart();
    context.run();
    auto connectStatus = connectFuture.wait_for(std::chrono::milliseconds(timeout));
    if (connectStatus == std::future_status::ready && connectFuture.get()) {
        uint64_t len = domain.length();
        st::utils::copy(domain.data(), sBuffer, len);
        sBuffer[len] = '\r';
        sBuffer[len + 1] = '\n';
        auto sendFuture = socket.async_send(buffer(sBuffer, (len + 2) * sizeof(char)), use_future([&](boost::system::error_code error, size_t size) {
                                                if (error) {
                                                    Logger::ERROR << logTag << "send domain error!" << error.message() << END;
                                                }
                                                return !error;
                                            }));
        context.restart();
        context.run();
        auto sendStatus = sendFuture.wait_for(std::chrono::milliseconds((time::now() - begin)));
        if (sendStatus == std::future_status::ready && sendFuture.get()) {
            bool end = false;
            string result = "";
            while (!end) {
                auto receiveFuture = socket.async_read_some(buffer(sBuffer, 1024 * sizeof(char)),
                                                            use_future([&](boost::system::error_code error, size_t size) {
                                                                if (error) {
                                                                    end = true;
                                                                    bool isEOF = error.category() == error::misc_category && error == error::misc_errors::eof;
                                                                    if (!isEOF) {
                                                                        Logger::ERROR << logTag << "send domain error!" << error.message() << END;
                                                                    }
                                                                }
                                                                sBuffer[size] = '\0';
                                                                return size;
                                                            }));
                context.restart();
                context.run();
                uint64_t tim = time::now() - begin;
                auto receiveStatus = receiveFuture.wait_for(std::chrono::milliseconds(tim));
                if (receiveStatus == std::future_status::ready && receiveFuture.get()) {
                    result += sBuffer;
                } else if (!end) {
                    end = true;
                    Logger::ERROR << logTag << "reveive timeout" << END;
                } else {
                    return move(result);
                }
            }

        } else {
            if (sendStatus != std::future_status::ready) {
                Logger::ERROR << logTag << "send domain timeout" << END;
            }
        }
    } else {
        if (connectStatus != std::future_status::ready) {
            Logger::ERROR << logTag << "connect timeout" << END;
        }
    }
    return move("");
}
