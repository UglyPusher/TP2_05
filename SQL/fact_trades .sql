-- Фактические трейды (тики), уже нормализованные под нашу систему
create table fact_trades (
    -- Наш внутренний идентификатор строки (биржи могут давать неуникальные или пустые trade_id)
    id bigserial primary key,

    -- Время сделки по бирже (или уже нормированное в UTC)
    ts timestamptz not null,

    -- Биржа, которая прислала сделку
    exchange_code text not null references ref_exchanges(code),

    -- Имя инструмента ТАК, КАК его называет биржа
    exchange_symbol text not null,

    -- Наш системный инструмент (обобщённый)
    instrument_code text not null references ref_instruments(code),

    -- Биржевой идентификатор сделки (как прислали). Может быть NULL или повторяться на разных символах.
    trade_id text,

    -- Цена и объём сделки
    price numeric(20,10) not null,
    qty   numeric(20,10) not null,

    -- Направление, если биржа его прислала: 'B' / 'S'
    side  char(1),

    -- Сырой payload на случай, что что-то захочется допарсить
    source jsonb,

    -- Связь с маппингом биржи (обеспечивает, что такой exchange+symbol у нас вообще существует)
    foreign key (exchange_code, exchange_symbol)
        references ref_exchange_instruments (exchange_code, exchange_symbol)
);

-- Частый запрос: "все сделки по системному инструменту за период"
create index idx_fact_trades_instr_ts
    on fact_trades (instrument_code, ts desc);

-- Частый запрос: "все сделки по биржевому символу за период"
create index idx_fact_trades_exch_sym_ts
    on fact_trades (exchange_code, exchange_symbol, ts desc);

-- Иногда просто по времени
create index idx_fact_trades_ts
    on fact_trades (ts desc);

-- Комментарии
comment on table fact_trades
  is 'Исторические сделки (тики), связанные и с биржей, и с биржевым инструментом, и с нашим системным инструментом.';

comment on column fact_trades.ts
  is 'Время сделки в UTC (желательно), используется для партиционирования и выборок.';

comment on column fact_trades.exchange_code
  is 'Код биржи (ref_exchanges), откуда пришёл тик.';

comment on column fact_trades.exchange_symbol
  is 'Имя инструмента на бирже, как в их WS/REST. Связано с ref_exchange_instruments.';

comment on column fact_trades.instrument_code
  is 'Наш обобщённый инструмент (ref_instruments), чтобы собрать сделки с разных бирж.';

comment on column fact_trades.trade_id
  is 'Биржевой идентификатор сделки, может быть пустым или неполностью уникальным.';

comment on column fact_trades.source
  is 'Сырой JSON с биржи (редкие поля, которые не стали выносить в колонки).';
