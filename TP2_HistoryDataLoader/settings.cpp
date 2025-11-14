#include "settings.hpp"
#include <fstream>
#include <sstream>

bool load_settings(const std::string& path, Settings& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (key == "conn_string") out.conn_string = val;
        else if (key == "log_file") out.log_file = val;
        else if (key == "log_level") out.log_level = std::stoi(val);
        else if (key == "batch_size") out.batch_size = std::stoi(val);
    }
    return !out.conn_string.empty();
}
