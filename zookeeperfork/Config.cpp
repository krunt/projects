#include "Config.h"
#include <fstream>
#include <string.h>

void Config::initialize(const std::string &filename) {
    char line[1024];
    std::fstream stream(filename.c_str(), std::ios_base::in);
    while (stream.getline(line, sizeof(line))) {
        line[sizeof(line) - 1] = 0;
        char *p = strchr(line, '=');
        if (!p) {
            continue;
        }
        *p = 0;
        char *v = ++p;
        std::string key = strip_string(line); 
        std::string value = strip_string(v);

        if (key == "myid") {
            myid_ = boost::lexical_cast<int>(value.c_str());
        else if (key == "client_port") {
            client_port_ = boost::lexical_cast<int>(value.c_str());
        } else if (key.substr(0, 6) == "server") {
            assert(key.find('.') != std::string::npos);
            std::string string_id = key.substr(key.find('.') + 1);
            int id = boost::lexical_cast<int>(string_id.c_str());
            servers_.push_back(std::make_pair(id, ServerHostPort(value)));
        }
    }
    assert(client_port_ != -1);
}
