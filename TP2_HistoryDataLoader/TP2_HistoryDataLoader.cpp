#include <iostream>
#include <pqxx/pqxx>
#include "settings.hpp"
#include "logger.hpp"
#include "db.hpp"
#include "loader_bybit.hpp"

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "usage: loader <exchange_code> <exchange_symbol> <data_file> <settings_file>\n";
        return 1;
    }
    std::string exchange_code = argv[1];
    std::string exchange_symbol = argv[2];
    std::string data_file = argv[3];
    std::string settings_file = argv[4];

    Settings s;
    if (!load_settings(settings_file, s)) {
        std::cerr << "cannot read settings: " << settings_file << "\n";
        return 2;
    }
    Logger log(s.log_file, static_cast<Logger::Level>(s.log_level));
    log.info("started loader for " + exchange_code + " " + exchange_symbol);
    try {
        pqxx::connection c{ s.conn_string };
        std::string instrument_code = fetch_instrument_code(c, exchange_code, exchange_symbol);
        if (instrument_code.empty()) {
            log.error("instrument mapping not found for " + exchange_code + "/" + exchange_symbol);
            return 3;
        }
        if (exchange_code == "BYBIT") {
            import_bybit_trades(c, exchange_code, exchange_symbol, instrument_code, data_file, s.batch_size, log);
        }
        else {
            log.error("unsupported exchange: " + exchange_code);
            return 4;
        }
    }
    catch (const std::exception& e) {
        log.error(std::string("exception: ") + e.what());
        return 5;
    }
    return 0;
}
