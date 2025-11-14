#pragma once
#include <string>

struct Settings {
    std::string conn_string;
    std::string log_file = "loader.log";
    int log_level = 2;
    int batch_size = 500;
};

bool load_settings(const std::string& path, Settings& out);
