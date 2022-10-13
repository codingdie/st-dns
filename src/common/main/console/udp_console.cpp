//
// Created by codingdie on 10/13/22.
//

#include "udp_console.h"

namespace st {
    namespace console {
        udp_console::udp_console(const std::string &ip, uint16_t port)
            : ip(ip), port(port), ic(), iw(new boost::asio::io_context::work(ic)),
              socket(udp::socket(ic, udp::endpoint(boost::asio::ip::make_address_v4(ip), port))) {
            th = new std::thread([=]() {
                this->ic.run();
            });
        }
        void udp_console::start() {
            receive();
        }
        void udp_console::receive() {
            socket.async_receive_from(buffer(command_buffer, 1024), client_endpoint,
                                      [=](boost::system::error_code code, size_t size) {
                                          command_buffer[size] = '\0';
                                          string command = command_buffer;
                                          logger::INFO << "receive command:" << command << END;
                                          auto parsed_command = parse_command(command);
                                          string result = impl(parsed_command.first, parsed_command.second);
                                          send_response(result);
                                      });
        }
        void udp_console::send_response(const string &result) {
            copy(result.c_str(), response_buffer, result.length());
            socket.async_send_to(buffer(response_buffer, result.length()), client_endpoint,
                                 [=](boost::system::error_code code, size_t size) {
                                     receive();
                                 });
        }
        udp_console::~udp_console() {
            ic.stop();
            delete iw;
            th->join();
            delete th;
        }
        pair<vector<std::string>, boost::program_options::variables_map> udp_console::parse_command(const string &command) {
            namespace po = boost::program_options;
            po::variables_map vm;
            auto splits = utils::strutils::split(command, " ");
            vector<string> commands;
            vector<string> options;
            bool is_option = false;
            uint16_t argc = 1;
            for (const auto &item : splits) {
                if (!is_option && item[0] != '-') {
                    commands.emplace_back(item);
                } else {
                    is_option = true;
                    options.emplace_back(item);
                    argc++;
                }
            }
            const char *argv[argc];
            argv[0] = "console";
            for (auto i = 1; i < argc; i++) {
                argv[i] = options[i - 1].c_str();
            }
            po::parsed_options parsed =
                    po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
            po::store(parsed, vm);
            po::notify(vm);
            return make_pair(commands, vm);
        }
    }// namespace console
}// namespace st