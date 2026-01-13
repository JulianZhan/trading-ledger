package com.trading.ledger.dto;

import lombok.*;

import java.math.BigDecimal;

@Getter
@Setter
@NoArgsConstructor
@AllArgsConstructor
@Builder
public class PositionResponse {
    private String accountId;
    private String symbol;
    private BigDecimal quantity;
    private BigDecimal averagePrice;
}
