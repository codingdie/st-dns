//
// Created by codingdie on 10/13/22.
//

#ifndef ST_UDP_CONSOLE_H
#define ST_UDP_CONSOLE_H
#include "st.h"
#include <functional>
#include <boost/program_options.hpp>
namespace st {
    namespace console {
        class udp_console {
        public:
            udp_console(const std::string &ip, uint16_t port);
            std::function<std::pair<bool, std::string>(const vector<std::string> &commands, const boost::program_options::variables_map &options)> impl;
            boost::program_options::options_description desc;

            void start();
            virtual ~udp_console();

        private:
            std::string ip;
            uint16_t port;
            io_context ic;
            boost::asio::io_context::work *iw;
            udp::socket socket;
            std::thread *th = nullptr;
            char command_buffer[1024];
            char response_buffer[10240];
            boost::asio::ip::udp::endpoint client_endpoint;

            void receive();
            pair<vector<std::string>, boost::program_options::variables_map> parse_command(const string &command);

            void send_response(const string &result);
        };

    }// namespace console
}// namespace st

#endif//ST_UDP_CONSOLE_H
