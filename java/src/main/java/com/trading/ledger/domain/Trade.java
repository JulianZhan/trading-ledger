package com.trading.ledger.domain;

import lombok.*;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Getter
@Setter
@NoArgsConstructor
@AllArgsConstructor
@ToString
public class Trade {

    private Long id;
    private UUID tradeId;
    private String accountId;
    private String symbol;
    private BigDecimal quantity;
    private BigDecimal price;
    private Side side;
    private Long timestampNs;
    private Instant createdAt;

    public enum Side {
        BUY, SELL
    }

    public Trade(UUID tradeId, String accountId, String symbol, BigDecimal quantity,
                 BigDecimal price, Side side, Long timestampNs) {
        this.tradeId = tradeId;
        this.accountId = accountId;
        this.symbol = symbol;
        this.quantity = quantity;
        this.price = price;
        this.side = side;
        this.timestampNs = timestampNs;
    }
}
