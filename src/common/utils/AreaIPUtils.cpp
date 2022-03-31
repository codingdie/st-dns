//
// Created by System Administrator on 2020/10/8.
//

#include "AreaIPUtils.h"
#include "FileUtils.h"
#include "Logger.h"
#include <iostream>
#include <boost/asio.hpp>
#include <queue>
#include <boost/locale.hpp>
#include <regex>
#include <boost/property_tree/json_parser.hpp>
using namespace boost::property_tree;


unordered_map<string, string> st::areaip::Manager::CN_AREA_2_AREA = {};
string st::areaip::Manager::CN_CCODE_JSON = "{\"阿富汗\": \"AF\", \"奥兰\": \"AX\", \"阿尔巴尼亚\": \"AL\", \"阿尔及利亚\": \"DZ\", \"美属萨摩亚\": \"AS\", \"安道尔\": \"AD\", \"安哥拉\": \"AO\", \"安圭拉\": \"AI\", \"南极洲\": \"AQ\", \"安提瓜和巴布达\": \"AG\", \"阿根廷\": \"AR\", \"亚美尼亚\": \"AM\", \"阿鲁巴\": \"AW\", \"澳大利亚\": \"AU\", \"奥地利\": \"AT\", \"阿塞拜疆\": \"AZ\", \"巴哈马\": \"BS\", \"巴林\": \"BH\", \"孟加拉国\": \"BD\", \"巴巴多斯\": \"BB\", \"白俄罗斯\": \"BY\", \"比利时\": \"BE\", \"伯利兹\": \"BZ\", \"贝宁\": \"BJ\", \"百慕大\": \"BM\", \"不丹\": \"BT\", \"玻利维亚\": \"BO\", \"荷兰加勒比区\": \"BQ\", \"波黑\": \"BA\", \"博茨瓦纳\": \"BW\", \"布韦岛\": \"BV\", \"巴西\": \"BR\", \"英属印度洋领地\": \"IO\", \"文莱\": \"BN\", \"保加利亚\": \"BG\", \"布基纳法索\": \"BF\", \"布隆迪\": \"BI\", \"佛得角\": \"CV\", \"柬埔寨\": \"KH\", \"喀麦隆\": \"CM\", \"加拿大\": \"CA\", \"开曼群岛\": \"KY\", \"中非\": \"CF\", \"乍得\": \"TD\", \"智利\": \"CL\", \"中国\": \"CN\", \"圣诞岛\": \"CX\", \"科科斯（基林）群岛\": \"CC\", \"哥伦比亚\": \"CO\", \"科摩罗\": \"KM\", \"刚果共和国\": \"CG\", \"刚果民主共和国\": \"CD\", \"库克群岛\": \"CK\", \"哥斯达黎加\": \"CR\", \"科特迪瓦\": \"CI\", \"克罗地亚\": \"HR\", \"古巴\": \"CU\", \"库拉索\": \"CW\", \"塞浦路斯\": \"CY\", \"捷克\": \"CZ\", \"丹麦\": \"DK\", \"吉布提\": \"DJ\", \"多米尼克\": \"DM\", \"多米尼加\": \"DO\", \"厄瓜多尔\": \"EC\", \"埃及\": \"EG\", \"萨尔瓦多\": \"SV\", \"赤道几内亚\": \"GQ\", \"厄立特里亚\": \"ER\", \"爱沙尼亚\": \"EE\", \"斯威士兰\": \"SZ\", \"埃塞俄比亚\": \"ET\", \"福克兰群岛\": \"FK\", \"法罗群岛\": \"FO\", \"斐济\": \"FJ\", \"芬兰\": \"FI\", \"法国\": \"FR\", \"法属圭亚那\": \"GF\", \"法属波利尼西亚\": \"PF\", \"法属南部和南极领地\": \"TF\", \"加蓬\": \"GA\", \"冈比亚\": \"GM\", \"格鲁吉亚\": \"GE\", \"德国\": \"DE\", \"加纳\": \"GH\", \"直布罗陀\": \"GI\", \"希腊\": \"GR\", \"格陵兰\": \"GL\", \"格林纳达\": \"GD\", \"瓜德罗普\": \"GP\", \"关岛\": \"GU\", \"危地马拉\": \"GT\", \"根西\": \"GG\", \"几内亚\": \"GN\", \"几内亚比绍\": \"GW\", \"圭亚那\": \"GY\", \"海地\": \"HT\", \"赫德岛和麦克唐纳群岛\": \"HM\", \"梵蒂冈\": \"VA\", \"洪都拉斯\": \"HN\", \"香港\": \"HK\", \"匈牙利\": \"HU\", \"冰岛\": \"IS\", \"印度\": \"IN\", \"印尼\": \"ID\", \"伊朗\": \"IR\", \"伊拉克\": \"IQ\", \"爱尔兰\": \"IE\", \"马恩岛\": \"IM\", \"以色列\": \"IL\", \"意大利\": \"IT\", \"牙买加\": \"JM\", \"日本\": \"JP\", \"泽西\": \"JE\", \"约旦\": \"JO\", \"哈萨克斯坦\": \"KZ\", \"肯尼亚\": \"KE\", \"基里巴斯\": \"KI\", \"朝鲜\": \"KP\", \"韩国\": \"KR\", \"科威特\": \"KW\", \"吉尔吉斯斯坦\": \"KG\", \"老挝\": \"LA\", \"拉脱维亚\": \"LV\", \"黎巴嫩\": \"LB\", \"莱索托\": \"LS\", \"利比里亚\": \"LR\", \"利比亚\": \"LY\", \"列支敦士登\": \"LI\", \"立陶宛\": \"LT\", \"卢森堡\": \"LU\", \"澳门\": \"MO\", \"马达加斯加\": \"MG\", \"马拉维\": \"MW\", \"马来西亚\": \"MY\", \"马尔代夫\": \"MV\", \"马里\": \"ML\", \"马耳他\": \"MT\", \"马绍尔群岛\": \"MH\", \"马提尼克\": \"MQ\", \"毛里塔尼亚\": \"MR\", \"毛里求斯\": \"MU\", \"马约特\": \"YT\", \"墨西哥\": \"MX\", \"密克罗尼西亚联邦\": \"FM\", \"摩尔多瓦\": \"MD\", \"摩纳哥\": \"MC\", \"蒙古\": \"MN\", \"黑山\": \"ME\", \"蒙特塞拉特\": \"MS\", \"摩洛哥\": \"MA\", \"莫桑比克\": \"MZ\", \"缅甸\": \"MM\", \"纳米比亚\": \"NA\", \"瑙鲁\": \"NR\", \"尼泊尔\": \"NP\", \"荷兰\": \"NL\", \"新喀里多尼亚\": \"NC\", \"新西兰\": \"NZ\", \"尼加拉瓜\": \"NI\", \"尼日尔\": \"NE\", \"尼日利亚\": \"NG\", \"纽埃\": \"NU\", \"诺福克岛\": \"NF\", \"北马其顿\": \"MK\", \"北马里亚纳群岛\": \"MP\", \"挪威\": \"NO\", \"阿曼\": \"OM\", \"巴基斯坦\": \"PK\", \"帕劳\": \"PW\", \"巴勒斯坦\": \"PS\", \"巴拿马\": \"PA\", \"巴布亚新几内亚\": \"PG\", \"巴拉圭\": \"PY\", \"秘鲁\": \"PE\", \"菲律宾\": \"PH\", \"皮特凯恩群岛\": \"PN\", \"波兰\": \"PL\", \"葡萄牙\": \"PT\", \"波多黎各\": \"PR\", \"卡塔尔\": \"QA\", \"留尼汪\": \"RE\", \"罗马尼亚\": \"RO\", \"俄罗斯\": \"RU\", \"卢旺达\": \"RW\", \"圣巴泰勒米\": \"BL\", \"圣赫勒拿、阿森松和特里斯坦-达库尼亚\": \"SH\", \"圣基茨和尼维斯\": \"KN\", \"圣卢西亚\": \"LC\", \"法属圣马丁\": \"MF\", \"圣皮埃尔和密克隆\": \"PM\", \"圣文森特和格林纳丁斯\": \"VC\", \"萨摩亚\": \"WS\", \"圣马力诺\": \"SM\", \"圣多美和普林西比\": \"ST\", \"沙特阿拉伯\": \"SA\", \"塞内加尔\": \"SN\", \"塞尔维亚\": \"RS\", \"塞舌尔\": \"SC\", \"塞拉利昂\": \"SL\", \"新加坡\": \"SG\", \"荷属圣马丁\": \"SX\", \"斯洛伐克\": \"SK\", \"斯洛文尼亚\": \"SI\", \"所罗门群岛\": \"SB\", \"索马里\": \"SO\", \"南非\": \"ZA\", \"南乔治亚和南桑威奇群岛\": \"GS\", \"南苏丹\": \"SS\", \"西班牙\": \"ES\", \"斯里兰卡\": \"LK\", \"苏丹\": \"SD\", \"苏里南\": \"SR\", \"斯瓦尔巴和扬马延\": \"SJ\", \"瑞典\": \"SE\", \"瑞士\": \"CH\", \"叙利亚\": \"SY\", \"台湾\": \"TW\", \"塔吉克斯坦\": \"TJ\", \"坦桑尼亚\": \"TZ\", \"泰国\": \"TH\", \"东帝汶\": \"TL\", \"多哥\": \"TG\", \"托克劳\": \"TK\", \"汤加\": \"TO\", \"特立尼达和多巴哥\": \"TT\", \"突尼斯\": \"TN\", \"土耳其\": \"TR\", \"土库曼斯坦\": \"TM\", \"特克斯和凯科斯群岛\": \"TC\", \"图瓦卢\": \"TV\", \"乌干达\": \"UG\", \"乌克兰\": \"UA\", \"阿联酋\": \"AE\", \"英国\": \"GB\", \"美国\": \"US\", \"美国本土外小岛屿\": \"UM\", \"乌拉圭\": \"UY\", \"乌兹别克斯坦\": \"UZ\", \"瓦努阿图\": \"VU\", \"委内瑞拉\": \"VE\", \"越南\": \"VN\", \"英属维尔京群岛\": \"VG\", \"美属维尔京群岛\": \"VI\", \"瓦利斯和富图纳\": \"WF\", \"西撒哈拉\": \"EH\", \"也门\": \"YE\", \"赞比亚\": \"ZM\", \"津巴布韦\": \"ZW\"}";
const int NET_QPS = 10;
namespace st {
    namespace areaip {
        static string NET_AREA_FILE = "/etc/area-ips/NET";
        using namespace st::utils;
        Manager Manager::instance;
        Manager &Manager::uniq() {
            return instance;
        }
        void Manager::initAreaCodeNameMap() {
            if (CN_AREA_2_AREA.empty()) {
                try {
                    ptree tree;
                    std::stringstream results(CN_CCODE_JSON);
                    read_json(results, tree);
                    for (auto it = tree.begin(); it != tree.end(); it++) {
                        auto key = it->first;
                        auto value = it->second.get_value<string>();
                        CN_AREA_2_AREA[key] = value;
                    }
                } catch (json_parser_error &e) {
                    Logger::ERROR << "initAreaCodeNameMap error! not valid json" << CN_CCODE_JSON << END;
                }
            }
        }

        Manager::Manager() {
            initAreaCodeNameMap();
            ioContext = new boost::asio::io_context();
            statTimer = new boost::asio::deadline_timer(*ioContext);
            ioContextWork = new boost::asio::io_context::work(*ioContext);
            th = new thread([=]() {
                ioContext->run();
            });
            vector<AreaIP> ips;
            ips.emplace_back(AreaIP::parse("192.168.0.0/16", "LAN"));
            ips.emplace_back(AreaIP::parse("10.0.0.0/8", "LAN"));
            ips.emplace_back(AreaIP::parse("172.16.0.0/16", "LAN"));
            ips.emplace_back(AreaIP::parse("0.0.0.0/8", "LAN"));
            defaultCaches.emplace("LAN", ips);
            syncNetAreaIP();
        }
        Manager::~Manager() {
            ioContext->stop();
            th->join();
            delete ioContext;
            delete ioContextWork;
            delete th;
            delete statTimer;
        }

        string Manager::getAreaCode(const string &areaReg) {
            string areaCode = areaReg;
            if (areaReg[0] == '!') {
                areaCode = areaReg.substr(1, areaReg.size() - 1);
            }
            transform(areaCode.begin(), areaCode.end(), areaCode.begin(), ::toupper);
            return areaCode;
        }
        string Manager::downloadAreaIPs(const string &areaCode) {
            string areaCodeLow = areaCode;
            transform(areaCodeLow.begin(), areaCodeLow.end(), areaCodeLow.begin(), ::tolower);
            string filePath = "/etc/area-ips/" + areaCode;
            if (!file::exit(filePath)) {
                file::createIfNotExits(filePath);
                string url = "https://raw.githubusercontent.com/herrbischoff/country-ip-blocks/master/ipv4/" +
                             areaCodeLow + ".cidr";
                if (shell::exec("wget -q " + url + " -O " + filePath)) {
                    return filePath;
                } else {
                    Logger::ERROR << areaCode << "ips file downloadn failed!" << url << END;
                    file::del(filePath);
                    return "";
                }
            }
            return filePath;
        }

        bool Manager::loadAreaIPs(const string &areaReg) {
            string areaCode = getAreaCode(areaReg);
            if (defaultCaches.find(areaCode) != defaultCaches.end()) {
                return true;
            }
            std::lock_guard<std::mutex> lg(defaultLock);
            if (defaultCaches.find(areaCode) == defaultCaches.end()) {
                string dataPath = downloadAreaIPs(areaCode);
                if (dataPath.compare("") == 0) {
                    return false;
                }
                ifstream in(dataPath);
                string line;
                vector<AreaIP> &ips = defaultCaches[areaCode];
                if (in) {
                    while (getline(in, line)) {
                        if (!line.empty()) {
                            ips.emplace_back(AreaIP::parse(line, areaCode));
                        }
                    }
                    Logger::INFO << "load area" << areaCode << "ips" << ips.size() << "from" << dataPath << END;
                }
            }
            return true;
        }


        bool Manager::isAreaIP(const std::vector<string> &areas, const uint32_t &ip) {
            if (areas.empty()) {
                return true;
            }
            for (auto it = areas.begin(); it != areas.end(); it++) {
                string areaReg = *it;
                string areaCode = getAreaCode(areaReg);
                if (!loadAreaIPs(areaCode)) {
                    return false;
                }
            }
            auto ipArea = getArea(ip);
            if (ipArea == "default") {
                return false;
            }
            for (auto it = areas.begin(); it != areas.end(); it++) {
                string areaReg = *it;
                bool matchNot = areaReg[0] == '!';
                string areaCode = getAreaCode(areaReg);
                if (!matchNot && ipArea.compare(areaCode) == 0) {
                    return true;
                }
                if (matchNot && ipArea.compare(areaCode) != 0) {
                    return true;
                }
            }
            return false;
        }
        bool Manager::isAreaIP(const string &areaReg, const uint32_t &ip) {
            vector<string> areas({areaReg});
            return isAreaIP(areas, ip);
        }

        bool Manager::isAreaIP(const string &areaCode, const string &ip) {
            return isAreaIP(areaCode, st::utils::ipv4::strToIp(ip));
        }
        uint16_t area2Code(const string &areaCode) {
            if (areaCode.length() == 2 && areaCode[0] >= 'A' && areaCode[1] >= 'A' && areaCode[1] <= 'Z' &&
                areaCode[0] <= 'Z') {
                uint8_t firstCode = areaCode[0] - 'A';
                uint8_t secondCode = areaCode[1] - 'A';
                return firstCode * 26 + secondCode;
            }
            return 0;
        }
        string code2Area(uint16_t mark) {
            if (mark >= 0 && mark <= 26 * 26 - 1) {
                string area = "##";
                area[0] = (char) ('A' + mark / 26);
                area[1] = (char) ('A' + mark % 26);
                return area;
            }
            return "";
        }
        void Manager::asyncLoadIPInfo(const uint32_t &ip) {
            std::function<void(const uint32_t &ip)> doLoadIPInfo = [=](const uint32_t &ip) {
                uint64_t begin = st::utils::time::now();
                bool success = true;
                if (getArea(ip, netCaches) == "") {
                    AreaIP ipInfo;
                    bool success = loadIPInfo(ip, ipInfo);
                    APMLogger::perf("load-net-ipinfo", {{"success", to_string(success)}}, st::utils::time::now() - begin);
                    if (success) {
                        Logger::INFO << "async load ip info" << st::utils::ipv4::ipToStr(ip) << ipInfo.serialize() << END;
                        std::lock_guard<std::mutex> lg(netLock);
                        ofstream fs(NET_AREA_FILE, std::ios_base::out | std::ios_base::app);
                        if (fs.is_open()) {
                            fs << ipInfo.serialize() << "\n";
                            fs.flush();
                        }
                        this->netCaches[ipInfo.area].emplace_back(ipInfo);
                    } else {
                        Logger::ERROR << "async load ip info failed!" << st::utils::ipv4::ipToStr(ip) << END;
                    }
                } else {
                    Logger::INFO << "async load ip info skiped!" << st::utils::ipv4::ipToStr(ip) << END;
                }
                uint64_t cost = st::utils::time::now() - begin;
                uint64_t timeout = 1000L / NET_QPS;

                if (cost < timeout) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(timeout - cost));
                }
            };
            ioContext->post(boost::bind(doLoadIPInfo, ip));
        }

        bool Manager::isAreaIP(const string &areaCode, const uint32_t &ip, unordered_map<string, vector<AreaIP>> &caches) {
            //todo find fast
            auto iterator = caches.find(areaCode);
            if (iterator != caches.end()) {
                for (auto it = iterator->second.begin(); it != iterator->second.end(); it++) {
                    auto areaIP = *it;
                    if (ip <= areaIP.end && ip >= areaIP.start) {
                        return true;
                    }
                }
            }
            return false;
        }

        string Manager::getArea(const uint32_t &ip, unordered_map<string, vector<AreaIP>> &caches) {
            if (ip != 0) {
                for (auto it = caches.begin(); it != caches.end(); it++) {
                    string area = (*it).first;
                    if (isAreaIP(area, ip, caches)) {
                        return area;
                    }
                }
            }
            return "";
        }

        string Manager::getArea(const uint32_t &ip) {
            string area = "";
            if (ip != 0) {
                area = getArea(ip, netCaches);
                if (area == "") {
                    area = getArea(ip, defaultCaches);
                    asyncLoadIPInfo(ip);
                }
            }
            return area == "" ? "default" : area;
        }

        bool Manager::loadIPInfo(const uint32_t &ip, AreaIP &ipInfo) {
            string result;
            if (shell::exec("curl -s -H \"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/98.0.4758.80 Safari/537.36\" --location --request GET \"https://www.ip138.com/iplookup.asp?ip=" + st::utils::ipv4::ipToStr(ip) + "&action=2\"", result)) {
                result = boost::locale::conv::to_utf<char>(result.c_str(), std::string("gb2312"));
                std::smatch what;
                try {
                    std::regex expression("\\{\\\".*ip_c_list.*\\}");
                    if (std::regex_search(result, what, expression)) {
                        result = what[0].str();
                        ptree tree;
                        try {
                            std::stringstream results(result);
                            read_json(results, tree);
                            auto node = tree.get_child("ip_c_list");
                            if (!node.empty()) {
                                auto first = node.ordered_begin();
                                string areaCN = first->second.get("ct", "");
                                string subAreaCN = first->second.get("prov", "");
                                if (subAreaCN == "台湾省" || subAreaCN == "香港省") {
                                    areaCN = subAreaCN.substr(0, 6);
                                }
                                cout << areaCN << endl;
                                if (CN_AREA_2_AREA.find(areaCN) != CN_AREA_2_AREA.end()) {
                                    ipInfo.area = CN_AREA_2_AREA[areaCN];
                                    ipInfo.start = first->second.get<uint32_t>("begin", 0);
                                    ipInfo.end = first->second.get<uint32_t>("end", 0);
                                    return true;
                                }
                            }

                        } catch (json_parser_error &e) {
                            Logger::ERROR << " getAreaFromNet not valid json" << result << END;
                        }
                    }
                } catch (const std::regex_error &e) {
                    std::cerr << e.what() << '\n';
                }
            }
            return false;
        }
        void Manager::syncNetAreaIP() {
            statTimer->expires_from_now(boost::posix_time::seconds(10));
            statTimer->async_wait([=](boost::system::error_code ec) {
                if (!ec) {
                    this->syncNetAreaIP();
                }
            });
            lock_guard<mutex> lockGuard(netLock);
            unordered_set<string> finalRecord;
            ifstream in(NET_AREA_FILE);
            if (!in) {
                return;
            }
            string line;
            while (getline(in, line)) {
                if (!line.empty()) {
                    finalRecord.emplace(line);
                }
            }
            in.close();
            for (auto it = netCaches.begin(); it != netCaches.end(); it++) {
                auto records = it->second;
                string area = it->first;
                for (auto it2 = records.begin(); it2 != records.end(); it2++) {
                    finalRecord.emplace(it2->serialize());
                }
            }
            ofstream fileStream(NET_AREA_FILE);
            if (fileStream.is_open()) {
                unordered_map<string, vector<AreaIP>> newCaches;

                for (auto it = finalRecord.begin(); it != finalRecord.end(); it++) {
                    fileStream << *it << "\n";
                    AreaIP areaIP = AreaIP::parse(*it);
                    newCaches[areaIP.area].emplace_back(areaIP);
                }
                fileStream.flush();
                fileStream.close();
                this->netCaches = newCaches;
                Logger::INFO << "sync net area ips sucess! count:" << finalRecord.size() << END;
            }
        }


    }// namespace areaip
}// namespace st