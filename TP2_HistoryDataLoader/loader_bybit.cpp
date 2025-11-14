#include "loader_bybit.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <ctime>
#include <chrono>

#include <pqxx/pqxx>

struct BybitRow {
    std::string trade_id;
    long long   ts_ms = 0;   // ms since epoch
    double      price = 0.0;
    double      qty = 0.0;
    char        side = 0;   // 'B' / 'S'
};

// простенький сплитер + трим
static bool split_csv(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    std::stringstream ss(line);
    std::string col;
    while (std::getline(ss, col, ',')) {
        // trim right
        while (!col.empty() &&
            (col.back() == ' ' || col.back() == '\r' ||
                col.back() == '\n' || col.back() == '\t'))
            col.pop_back();
        // trim left
        while (!col.empty() && (col.front() == ' ' || col.front() == '\t'))
            col.erase(col.begin());
        out.push_back(col);
    }
    return !out.empty();
}

static bool try_stoll(const std::string& s, long long& out) {
    if (s.empty()) return false;
    try {
        out = std::stoll(s);
        return true;
    }
    catch (...) {
        return false;
    }
}

static bool try_stod(const std::string& s, double& out) {
    if (s.empty()) return false;
    try {
        out = std::stod(s);
        return true;
    }
    catch (...) {
        return false;
    }
}

// парсим строку BYBIT: id,timestamp,price,volume,side,(ignored)
static bool parse_bybit_line(const std::string& line, BybitRow& r, long long line_no, Logger& log) {
    std::vector<std::string> cols;
    if (!split_csv(line, cols)) {
        log.warn("line " + std::to_string(line_no) + ": cannot split csv");
        return false;
    }
    if (cols.size() < 5) {
        log.warn("line " + std::to_string(line_no) + ": expected 5 columns, got " + std::to_string(cols.size()));
        return false;
    }

    r = BybitRow{};
    r.trade_id = cols[0];

    long long ts_ms;
    if (!try_stoll(cols[1], ts_ms)) {
        log.warn("line " + std::to_string(line_no) + ": invalid timestamp: '" + cols[1] + "'");
        return false;
    }
    r.ts_ms = ts_ms;

    double price;
    if (!try_stod(cols[2], price)) {
        log.warn("line " + std::to_string(line_no) + ": invalid price: '" + cols[2] + "'");
        return false;
    }
    r.price = price;

    double qty;
    if (!try_stod(cols[3], qty)) {
        log.warn("line " + std::to_string(line_no) + ": invalid qty: '" + cols[3] + "'");
        return false;
    }
    r.qty = qty;

    if (cols[4] == "buy" || cols[4] == "BUY")
        r.side = 'B';
    else if (cols[4] == "sell" || cols[4] == "SELL")
        r.side = 'S';
    else
        r.side = 0; // неизвестный side

    return true;
}

// ms → "YYYY-MM-DD HH:MM:SS.mmm+00"
static std::string ms_to_ts_utc(long long ms) {
    long long sec = ms / 1000;
    long long msec = ms % 1000;
    std::time_t tt = static_cast<std::time_t>(sec);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << msec
        << "+00";
    return os.str();
}

void import_bybit_trades(
    pqxx::connection& c,
    const std::string& exchange_code,
    const std::string& exchange_symbol,
    const std::string& instrument_code,
    const std::string& data_file,
    int batch_size,
    Logger& log)
{
    std::ifstream f(data_file);
    if (!f.is_open()) {
        log.error("cannot open data file: " + data_file);
        return;
    }

    std::string line;
    // header
    if (!std::getline(f, line)) {
        log.warn("empty data file");
        return;
    }

    if (batch_size <= 0) batch_size = 1000;

    pqxx::work w{ c };

    long long line_no = 1; // header
    long long inserted = 0;
    long long skipped = 0;

    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty())
            continue;

        BybitRow row;
        if (!parse_bybit_line(line, row, line_no, log)) {
            ++skipped;
            continue;
        }

        // валидация
        if (row.trade_id.empty()) {
            log.warn("line " + std::to_string(line_no) + ": empty trade_id, skipped");
            ++skipped;
            continue;
        }
        if (row.ts_ms <= 0) {
            log.warn("line " + std::to_string(line_no) + ": non-positive ts_ms, skipped");
            ++skipped;
            continue;
        }
        if (row.price <= 0.0) {
            log.warn("line " + std::to_string(line_no) + ": non-positive price=" + std::to_string(row.price) + ", skipped");
            ++skipped;
            continue;
        }
        if (row.qty <= 0.0) {
            log.warn("line " + std::to_string(line_no) + ": non-positive qty=" + std::to_string(row.qty) + ", skipped");
            ++skipped;
            continue;
        }
        if (row.side == 0) {
            log.warn("line " + std::to_string(line_no) + ": empty or invalid side, skipped");
            ++skipped;
            continue;
        }

        std::string ts_str = ms_to_ts_utc(row.ts_ms);
        std::string side_str(1, row.side);

        // INSERT одной строкой, но всё через quote()
        std::string sql =
            "insert into fact_trades "
            "(ts, exchange_code, exchange_symbol, instrument_code, trade_id, price, qty, side, source) values (" +
            w.quote(ts_str) + "," +
            w.quote(exchange_code) + "," +
            w.quote(exchange_symbol) + "," +
            w.quote(instrument_code) + "," +
            w.quote(row.trade_id) + "," +
            w.quote(row.price) + "," +
            w.quote(row.qty) + "," +
            w.quote(side_str) + ", '{}'::jsonb)";

        w.exec(sql);
        ++inserted;

        if (inserted % batch_size == 0) {
            log.info("progress: inserted=" + std::to_string(inserted) +
                ", skipped=" + std::to_string(skipped));
        }
    }

    w.commit();

    log.info("import finished, inserted=" + std::to_string(inserted) +
        ", skipped=" + std::to_string(skipped));
}
