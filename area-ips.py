#!/usr/bin/python
# -*- coding: UTF-8 -*-
import os.path
import urllib

from bs4 import BeautifulSoup


def download(area_code):
    dir = "area-ips/"
    filepath = dir + area_code
    if os.path.isfile(filepath):
        return
    response = urllib.urlopen('http://ip.bczs.net/area/' + area_code)
    html = response.read()
    soup = BeautifulSoup(html, features="html.parser")
    ips = [];
    for tr in soup.select("#result table tbody tr"):
        tds = tr.select("td")
        if len(tds) == 3:
            ips.append(tds[0].string + " " + tds[1].string + "\n")
    if len(ips) > 0:
        if not os.path.isdir(dir):
            os.mkdir("%s" % dir)
        f = open(filepath, "wr");
        f.writelines(ips)
        f.flush()
        f.close()


area_codes = ["US", "CN", "JP", "DE", "GB", "KR", "BR", "FR", "CA", "AU", "IT", "NL", "RU", "IN", "TW", "ES", "SE",
              "MX", "ZA", "BE", "EG", "PL", "CH", "AR", "ID", "CO", "TR", "VN", "NO", "SG", "FI", "IR", "DK", "HK",
              "MA", "AT", "UA", "SA", "CL", "CZ", "TH", "RO", "SC", "TN", "IL", "NZ", "VE", "IE", "PT", "MY", "KE",
              "HU", "GR", "PH", "PK", "DZ", "BG", "AE", "PE", "NG", "KZ", "SK", "EC", "SI", "MU", "CR", "LT", "UY",
              "HR", "RS", "GH", "KW", "SD", "LV", "BY", "BD", "PA", "CI", "ZM", "DO", "LU", "UG", "EE", "MD", "GE",
              "AO", "SY", "BO", "PY", "TZ", "OM", "PR", "CY", "IS", "QA", "BA", "AZ", "PS", "MK", "IQ", "CM", "SV",
              "JO", "MT", "AM", "GT", "LB", "MG", "LK", "NP", "HN", "MW", "TT", "NA", "BH", "MZ", "LY", "GA", "AL",
              "NI", "SN", "KH", "RE", "ET", "TG", "MO", "BF", "RW", "KG", "GM", "CU", "MN", "UZ", "ME", "GU", "BN",
              "JM", "YE", "MM", "CW", "GI", "BB", "BJ", "CD", "BZ", "AF", "HT", "NC", "BW", "FJ", "CG", "BS", "ZW",
              "LS", "VI", "LI", "BM", "LR", "AW", "SL", "ML", "MV", "LA", "SR", "KY", "TJ", "MC", "GY", "VG", "JE",
              "PG", "PF", "AG", "AD", "SZ", "DJ", "SO", "FO", "MR", "NE", "BI", "SM", "GL", "SX", "BT", "GN", "IM",
                 "CV", "TD", "GG", "BQ", "GF", "SS", "LC", "GP", "GQ", "WS", "VU", "TL", "MP", "TM", "KN", "MQ", "SB",
                 "VA", "DM", "TO", "TC", "NR", "MF", "GD", "ST", "VC", "AI", "FM", "CK", "TV", "PW", "GW", "KM", "AS",
                 "CF", "KI", "PM", "MH", "ER", "FK", "WF", "IO", "YT", "NU", "NF", "TK", "BL", "MS", "AX", "KP", "AQ"];
for code in area_codes:
    download(code)
