package com.trading.ledger.service;

import com.trading.ledger.dto.TradeResponse;

public class TradeCreationResult {

    private final TradeResponse trade;
    private final boolean isNewlyCreated;

    public TradeCreationResult(TradeResponse trade, boolean isNewlyCreated) {
        this.trade = trade;
        this.isNewlyCreated = isNewlyCreated;
    }

    public TradeResponse getTrade() {
        return trade;
    }

    public boolean isNewlyCreated() {
        return isNewlyCreated;
    }
}
