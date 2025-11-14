#include "db.hpp"

std::string fetch_instrument_code(
    pqxx::connection& c,
    const std::string& exchange_code,
    const std::string& exchange_symbol)
{
    pqxx::work w{ c };

    auto r = w.exec(
        "select instrument_code from ref_exchange_instruments "
        "where exchange_code = " + w.quote(exchange_code) +
        " and exchange_symbol = " + w.quote(exchange_symbol));

    w.commit();

    if (r.empty())
        return {};

    return r[0][0].as<std::string>();
}
