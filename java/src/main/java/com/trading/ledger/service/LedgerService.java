package com.trading.ledger.service;

import com.trading.ledger.domain.LedgerEntry;
import com.trading.ledger.domain.Trade;
import com.trading.ledger.mapper.LedgerEntryMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.List;

@Service
public class LedgerService {

    private static final Logger logger = LoggerFactory.getLogger(LedgerService.class);

    private final LedgerEntryMapper ledgerEntryMapper;

    public LedgerService(LedgerEntryMapper ledgerEntryMapper) {
        this.ledgerEntryMapper = ledgerEntryMapper;
    }

    /**
     * Generate double-entry ledger entries for a trade.
     *
     * Double-entry accounting invariant: Every trade creates 2 entries that balance to 0.
     * Example: BUY 100 AAPL @ $150
     *   - DEBIT:  +$15,000 (cash decreases)
     *   - CREDIT: -$15,000 (stock increases)
     *   - SUM: $15,000 + (-$15,000) = 0
     */
    @Transactional
    public List<LedgerEntry> generateEntries(Trade trade) {
        logger.debug("Generating ledger entries for trade: {}", trade.getTradeId());
        BigDecimal amount = trade.getQuantity().multiply(trade.getPrice());

        // Create DEBIT entry
        LedgerEntry debitEntry = new LedgerEntry(
                trade.getTradeId(),
                trade.getAccountId(),
                LedgerEntry.EntryType.DEBIT,
                amount, 
                trade.getTimestampNs()
        );

        // Create CREDIT entry
        LedgerEntry creditEntry = new LedgerEntry(
                trade.getTradeId(),
                trade.getAccountId(),
                LedgerEntry.EntryType.CREDIT,
                amount,
                trade.getTimestampNs()
        );

        // Save both entries atomically
        ledgerEntryMapper.insertAll(List.of(debitEntry, creditEntry));

        // Validate double-entry invariant: SUM(DEBIT) + SUM(CREDIT) = 0
        BigDecimal sum = ledgerEntryMapper.sumEntriesByTradeId(trade.getTradeId());
        if (sum.compareTo(BigDecimal.ZERO) != 0) {
            logger.error("Double-entry invariant violated for trade {}: sum = {}", trade.getTradeId(), sum);
            throw new IllegalStateException("Double-entry accounting invariant violated: sum = " + sum);
        }

        logger.info("Generated 2 ledger entries for trade {}, balance: {}",
                trade.getTradeId(), sum);

        return List.of(debitEntry, creditEntry);
    }
}
