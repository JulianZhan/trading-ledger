#pragma once

#include "Event.h"
#include <string>
#include <unordered_map>
#include <iostream>

namespace trading_ledger {

/**
 * Double-entry accounting validator
 *
 * Validates that debits and credits balance for each trade.
 * Accumulates ledger entries and checks invariant: SUM(debits) = SUM(credits)
 */
class DoubleEntryValidator {
public:
    /**
     * Process an event
     * Currently validates TRADE_CREATED events
     */
    void processEvent(const Event& event);

    /**
     * Get validation statistics
     */
    struct Stats {
        size_t trades_validated = 0;
        size_t validation_errors = 0;
        size_t events_processed = 0;
    };

    Stats getStats() const { return stats_; }

    /**
     * Print validation summary
     */
    void printSummary(std::ostream& out = std::cout) const;

private:
    Stats stats_;

    // Track per-trade ledger state (for future multi-event validation)
    // Currently we validate complete trades in single event
    struct TradeState {
        double debit_total = 0.0;
        double credit_total = 0.0;
        int entry_count = 0;
    };

    std::unordered_map<std::string, TradeState> trade_states_;

    // Validate a TRADE_CREATED event
    void validateTradeCreated(const Event& event);

    // Extract trade_id from JSON payload (simple parsing)
    std::string extractTradeId(const std::string& json_payload) const;
};

}  // namespace trading_ledger
