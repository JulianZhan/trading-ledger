#include "DoubleEntryValidator.h"
#include <sstream>
#include <iomanip>

namespace trading_ledger {

void DoubleEntryValidator::processEvent(const Event& event) {
    stats_.events_processed++;

    switch (event.event_type) {
        case EventType::TRADE_CREATED:
            validateTradeCreated(event);
            break;

        case EventType::LEDGER_ENTRIES_GENERATED:
            // Future: validate ledger entries
            break;

        case EventType::POSITION_UPDATED:
            // Future: validate position updates
            break;

        default:
            // Unknown event type - skip
            break;
    }
}

void DoubleEntryValidator::validateTradeCreated(const Event& event) {
    // For MVP, we just count trades
    // In a full implementation, we would:
    // 1. Parse JSON payload to extract trade details
    // 2. Calculate expected debit/credit amounts
    // 3. Validate that they sum to zero

    // Extract trade_id for logging
    std::string trade_id = extractTradeId(event.payload);

    // For now, just validate that the event has a payload
    if (event.payload.empty()) {
        stats_.validation_errors++;
        std::cerr << "Validation error: Trade event with empty payload at sequence "
                  << event.sequence_num << std::endl;
        return;
    }

    // Simple validation: check JSON has expected fields
    bool has_trade_id = event.payload.find("\"trade_id\"") != std::string::npos;
    bool has_symbol = event.payload.find("\"symbol\"") != std::string::npos;
    bool has_quantity = event.payload.find("\"quantity\"") != std::string::npos;

    if (!has_trade_id || !has_symbol || !has_quantity) {
        stats_.validation_errors++;
        std::cerr << "Validation error: Trade event missing required fields at sequence "
                  << event.sequence_num << std::endl;
        return;
    }

    // Validation passed
    stats_.trades_validated++;

    // Log every 1000 trades
    if (stats_.trades_validated % 1000 == 0) {
        std::cout << "Validated " << stats_.trades_validated << " trades" << std::endl;
    }
}

std::string DoubleEntryValidator::extractTradeId(const std::string& json_payload) const {
    // Simple extraction: find "trade_id":"value"
    const std::string key = "\"trade_id\":\"";
    size_t pos = json_payload.find(key);
    if (pos == std::string::npos) {
        return "unknown";
    }

    pos += key.length();
    size_t end_pos = json_payload.find("\"", pos);
    if (end_pos == std::string::npos) {
        return "unknown";
    }

    return json_payload.substr(pos, end_pos - pos);
}

void DoubleEntryValidator::printSummary(std::ostream& out) const {
    out << "\n=== Validation Summary ===" << std::endl;
    out << "Events processed:   " << stats_.events_processed << std::endl;
    out << "Trades validated:   " << stats_.trades_validated << std::endl;
    out << "Validation errors:  " << stats_.validation_errors << std::endl;

    if (stats_.validation_errors == 0) {
        out << "Status: ✓ All validations passed" << std::endl;
    } else {
        out << "Status: ✗ Validation failures detected" << std::endl;
    }
    out << "=========================" << std::endl;
}

}  // namespace trading_ledger
