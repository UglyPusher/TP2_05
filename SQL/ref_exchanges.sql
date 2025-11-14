-- Справочник торговых площадок (бирж)
create table ref_exchanges (
    -- Короткий стабильный код: 'BYBIT', 'BINANCE', 'OKX'
    code text primary key,

    -- Человекочитаемое имя
    name text not null,

    -- Статус подключения/использования: active / disabled / test
    status text not null default 'active',

    -- Базовый REST-эндпоинт (если хочешь хранить тут)
    rest_base_url text,

    -- Базовый WS-эндпоинт (публичный)
    ws_base_url text
);

-- Быстрые выборки "все активные"
create index idx_ref_exchanges_status on ref_exchanges(status);

-- Комментарии
comment on table ref_exchanges is 'Справочник бирж/торговых площадок: код, название, базовые URL, статус.';
comment on column ref_exchanges.code is 'Системный короткий код биржи. Основной ключ.';
comment on column ref_exchanges.name is 'Полное имя биржи.';
comment on column ref_exchanges.status is 'Статус использования биржи: active/disabled/test.';
comment on column ref_exchanges.rest_base_url is 'Базовый REST-URL биржи (публичные/прямые запросы).';
comment on column ref_exchanges.ws_base_url is 'Базовый WebSocket-URL биржи.';
