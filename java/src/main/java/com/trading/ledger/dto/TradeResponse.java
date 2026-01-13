package com.trading.ledger.dto;

import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.math.BigDecimal;
import java.time.Instant;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class TradeResponse {

    private String tradeId;
    private String accountId;
    private String symbol;
    private BigDecimal quantity;
    private BigDecimal price;
    private String side;
    private Long timestampNs;
    private Instant createdAt;
}
