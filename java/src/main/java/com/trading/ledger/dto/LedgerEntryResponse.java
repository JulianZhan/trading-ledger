package com.trading.ledger.dto;

import com.trading.ledger.domain.LedgerEntry;
import lombok.*;

import java.math.BigDecimal;
import java.time.Instant;

@Getter
@Setter
@NoArgsConstructor
@AllArgsConstructor
@Builder
public class LedgerEntryResponse {
    private Long id;
    private String tradeId;
    private String accountId;
    private String entryType;
    private BigDecimal amount;
    private Long timestampNs;
    private Instant createdAt;

    public static LedgerEntryResponse from(LedgerEntry entry) {
        return LedgerEntryResponse.builder()
                .id(entry.getId())
                .tradeId(entry.getTradeId())
                .accountId(entry.getAccountId())
                .entryType(entry.getEntryType().name())
                .amount(entry.getAmount())
                .timestampNs(entry.getTimestampNs())
                .createdAt(entry.getCreatedAt())
                .build();
    }
}
