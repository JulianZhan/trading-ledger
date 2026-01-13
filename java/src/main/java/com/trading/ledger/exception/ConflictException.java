package com.trading.ledger.exception;

/**
 * Exception thrown when a trade with the same trade_id but different payload is submitted.
 * This enforces idempotency - same trade_id with different data is rejected with 409 Conflict.
 */
public class ConflictException extends RuntimeException {

    public ConflictException(String message) {
        super(message);
    }

    public ConflictException(String message, Throwable cause) {
        super(message, cause);
    }
}
