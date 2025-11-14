#pragma once
#include <string>
#include <pqxx/pqxx>
#include "logger.hpp"

void import_bybit_trades(
    pqxx::connection& c,
    const std::string& exchange_code,
    const std::string& exchange_symbol,
    const std::string& instrument_code,
    const std::string& data_file,
    int batch_size,
    Logger& log);
