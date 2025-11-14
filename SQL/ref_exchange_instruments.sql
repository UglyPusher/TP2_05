-- Маппинг биржевых инструментов на системные
create table ref_exchange_instruments (
    -- Биржа: BYBIT, BINANCE, OKX...
    exchange_code text not null references ref_exchanges(code),

    -- Как инструмент называется на самой бирже: 'ETHUSDT', 'BTC-USDT', 'ETHUSDT.P'
    exchange_symbol text not null,

    -- Как этот инструмент называется у нас в системе (обобщённый)
    instrument_code text not null references ref_instruments(code),

    -- Статус правила/инструмента на бирже
    status text not null default 'active',

    -- Точность цены на бирже (если отличается от системной)
    price_scale smallint,

    -- Точность количества на бирже (если отличается)
    qty_scale smallint,

    -- Доп. настройки/ограничения биржи по этому инструменту (minQty, stepSize, filters...)
    settings jsonb,

    primary key (exchange_code, exchange_symbol)
);

-- Быстро найти все биржевые названия по системному инструменту
create index idx_rei_instrument on ref_exchange_instruments(instrument_code);

-- Быстро получить все инструменты конкретной биржи
create index idx_rei_exchange on ref_exchange_instruments(exchange_code);

-- Комментарии
comment on table ref_exchange_instruments
  is 'Сопоставление биржевых инструментов (как на бирже) с системными инструментами (ref_instruments).';

comment on column ref_exchange_instruments.exchange_code
  is 'Код биржи (ref_exchanges).';

comment on column ref_exchange_instruments.exchange_symbol
  is 'Имя инструмента на бирже, как приходит в публичных/WS-данных.';

comment on column ref_exchange_instruments.instrument_code
  is 'Системный обобщённый инструмент (ref_instruments), к которому мы маппим этот биржевой.';

comment on column ref_exchange_instruments.status
  is 'Статус использования биржевого инструмента: active/disabled/test.';

comment on column ref_exchange_instruments.price_scale
  is 'Количество знаков после запятой у цены на ЭТОЙ бирже. Нужно для нормализации.';

comment on column ref_exchange_instruments.qty_scale
  is 'Количество знаков после запятой у количества/объёма на ЭТОЙ бирже.';

comment on column ref_exchange_instruments.settings
  is 'JSON с биржевыми ограничениями: шаг цены, шаг количества, минимумы и т.п.';
