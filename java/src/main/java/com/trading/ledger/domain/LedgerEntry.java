package com.trading.ledger.domain;

import lombok.*;

import java.math.BigDecimal;
import java.time.Instant;

@Getter
@Setter
@NoArgsConstructor
@AllArgsConstructor
@ToString
public class LedgerEntry {

    private Long id;
    private String tradeId;
    private String accountId;
    private EntryType entryType;
    private BigDecimal amount;
    private Long timestampNs;
    private Instant createdAt;

    public enum EntryType {
        DEBIT, CREDIT
    }

    public LedgerEntry(String tradeId, String accountId, EntryType entryType,
                       BigDecimal amount, Long timestampNs) {
        this.tradeId = tradeId;
        this.accountId = accountId;
        this.entryType = entryType;
        this.amount = amount;
        this.timestampNs = timestampNs;
    }
}
