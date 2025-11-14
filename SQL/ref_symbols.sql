-- Справочник символов (валют и криптовалют)
create table ref_symbols (
    -- Короткий стабильный код: 'BTC', 'USDT', 'EUR'
    code text primary key,

    -- Человекочитаемое имя
    name text not null,

    -- Тип символа: 'fiat', 'crypto', 'metal', ... (можно расширять)
    kind text not null default 'crypto',

    -- Количество знаков после запятой, для нормализации
    scale smallint not null default 8,

    -- Опционально: ISO-код для фиата
    iso_code text
);

-- Быстрые выборки "все крипты", "все фиаты"
create index idx_ref_symbols_kind on ref_symbols(kind);

-- Если хочешь искать по имени
create index idx_ref_symbols_name on ref_symbols(name);

-- Комментарии
comment on table ref_symbols is 'Справочник символов (валют и криптовалют), используется как системный уровень, не биржевой.';
comment on column ref_symbols.code is 'Системный короткий код символа. Основной ключ.';
comment on column ref_symbols.name is 'Полное имя символа (Bitcoin, United States Dollar).';
comment on column ref_symbols.kind is 'Классификация символа: fiat/crypto/...';
comment on column ref_symbols.scale is 'Количество знаков после запятой для стандартного представления/нормализации.';
comment on column ref_symbols.iso_code is 'ISO 4217 для фиатных валют, если применимо.';
