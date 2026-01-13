-- Stores all trade submissions with idempotency enforcement via unique trade_id
CREATE TABLE trades (
    id              BIGSERIAL PRIMARY KEY,
    trade_id        UUID NOT NULL UNIQUE,
    account_id      VARCHAR(64) NOT NULL,
    symbol          VARCHAR(16) NOT NULL,
    quantity        NUMERIC(18,8) NOT NULL CHECK (quantity > 0),
    price           NUMERIC(18,8) NOT NULL CHECK (price > 0),
    side            VARCHAR(4) NOT NULL CHECK (side IN ('BUY', 'SELL')),
    timestamp_ns    BIGINT NOT NULL,
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX idx_trades_account_id ON trades(account_id);
CREATE INDEX idx_trades_symbol ON trades(symbol);
CREATE INDEX idx_trades_created_at ON trades(created_at);

-- ============================================================================

-- Double-entry accounting: every trade generates exactly 2 entries (DEBIT + CREDIT)
CREATE TABLE ledger_entries (
    id              BIGSERIAL PRIMARY KEY,
    trade_id        UUID NOT NULL,
    account_id      VARCHAR(64) NOT NULL,
    entry_type      VARCHAR(6) NOT NULL CHECK (entry_type IN ('DEBIT', 'CREDIT')),
    amount          NUMERIC(18,8) NOT NULL,
    timestamp_ns    BIGINT NOT NULL,
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_ledger_trade FOREIGN KEY (trade_id) REFERENCES trades(trade_id) ON DELETE CASCADE
);
CREATE INDEX idx_ledger_trade_id ON ledger_entries(trade_id);
CREATE INDEX idx_ledger_account_id ON ledger_entries(account_id);

-- ============================================================================

CREATE TABLE positions (
    id              BIGSERIAL PRIMARY KEY,
    account_id      VARCHAR(64) NOT NULL,
    symbol          VARCHAR(16) NOT NULL,
    quantity        NUMERIC(18,8) NOT NULL,
    avg_price       NUMERIC(18,8),
    updated_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT uk_positions_account_symbol UNIQUE (account_id, symbol)
);

CREATE INDEX idx_positions_account_id ON positions(account_id);

-- ============================================================================
