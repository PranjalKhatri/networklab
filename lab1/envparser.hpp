#ifndef _ENV_PARSER_H_ 
#define _ENV_PARSER_H_

#include<map>
#include<string>
#include<fstream>
#include<algorithm>

std::map<std::string, std::string> 
parse_and_ret(std::string _file_loc = ".env", char _comment = '#') {
    std::map<std::string, std::string> ret;
    std::ifstream infile(_file_loc);
    
    if (!infile.is_open()) {
        return {};
    }

    std::string str;
    while (std::getline(infile, str)) {
        str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
        
        if (str.empty() || str[0] == _comment) {
            continue;
        }
        std::string::size_type _colon = str.find(':');
        if (_colon != std::string::npos) {
            std::string _key = str.substr(0, _colon);
            std::string _value = str.substr(_colon + 1);

            ret[_key] = _value;
        }
    }
    
    return ret;
}

#endif 