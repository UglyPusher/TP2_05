-- Обобщённый (системный) справочник инструментов
create table ref_instruments (
    -- Короткий код инструмента в системе: 'BTCUSDT', 'ETHUSD', 'SOLUSDT_PERP'
    code text primary key,

    -- Базовый символ (что торгуем): 'BTC', 'ETH', 'SOL'
    base_symbol_code text not null references ref_symbols(code),

    -- Котируемый символ (в чём цена): 'USDT', 'USD', 'EUR'
    quote_symbol_code text not null references ref_symbols(code),

    -- Тип инструмента: spot / perp / futures / option ...
    instrument_kind text not null default 'spot',

    -- Опциональное описание
    description text
);

-- Быстрый поиск по базовому/котируемому
create index idx_ref_instruments_base on ref_instruments(base_symbol_code);
create index idx_ref_instruments_quote on ref_instruments(quote_symbol_code);

-- Частый кейс: найти все инструменты по паре
create index idx_ref_instruments_base_quote
    on ref_instruments(base_symbol_code, quote_symbol_code);

-- Комментарии
comment on table ref_instruments is 'Обобщённые (системные) инструменты без привязки к биржам. Используются для нормализации биржевых инструментов.';
comment on column ref_instruments.code is 'Системный код инструмента (уникальный в пределах системы).';
comment on column ref_instruments.base_symbol_code is 'Базовый символ (что покупаем/продаём). Ссылка на ref_symbols.';
comment on column ref_instruments.quote_symbol_code is 'Котируемый символ (в чём выражена цена). Ссылка на ref_symbols.';
comment on column ref_instruments.instrument_kind is 'Тип инструмента: spot, perp, futures и т.п.';
comment on column ref_instruments.description is 'Произвольное описание/комментарий.';

-- (необязательно, но можно зажать типы)
alter table ref_instruments
  add constraint chk_ref_instruments_kind
  check (instrument_kind in ('spot','perp','futures','option'));
