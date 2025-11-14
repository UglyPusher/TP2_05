#pragma once
#include <string>
#include <pqxx/pqxx>

std::string fetch_instrument_code(
    pqxx::connection& c,
    const std::string& exchange_code,
    const std::string& exchange_symbol);
