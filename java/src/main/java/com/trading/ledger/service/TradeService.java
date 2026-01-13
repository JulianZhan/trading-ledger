package com.trading.ledger.service;

import com.trading.ledger.domain.Trade;
import com.trading.ledger.dto.CreateTradeRequest;
import com.trading.ledger.dto.TradeResponse;
import com.trading.ledger.exception.ConflictException;
import com.trading.ledger.mapper.TradeMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.Optional;
import java.util.UUID;

@Service
public class TradeService {

    private static final Logger logger = LoggerFactory.getLogger(TradeService.class);

    private final TradeMapper tradeMapper;
    private final LedgerService ledgerService;

    public TradeService(TradeMapper tradeMapper, LedgerService ledgerService) {
        this.tradeMapper = tradeMapper;
        this.ledgerService = ledgerService;
    }

    /**
     * Create a trade with idempotency guarantee.
     *
     * Idempotency behavior:
     * - First request: Create trade + ledger entries → 201 Created
     * - Retry (same payload): Return existing trade → 200 OK
     * - Retry (different payload): Throw ConflictException → 409 Conflict
     */
    @Transactional
    public TradeResponse createTrade(CreateTradeRequest request) {
        UUID tradeId = UUID.fromString(request.getTradeId());
        logger.debug("Processing trade creation request for tradeId: {}", tradeId);

        // Idempotency check: does this trade_id already exist?
        Optional<Trade> existing = tradeMapper.findByTradeId(tradeId);
        if (existing.isPresent()) {
            Trade existingTrade = existing.get();
            logger.debug("Trade {} already exists, checking payload match", tradeId);

            // Check if payload matches
            if (payloadMatches(existingTrade, request)) {
                logger.info("Trade {} already exists with same payload, returning existing (idempotent)", tradeId);
                return convertToResponse(existingTrade);
            } else {
                logger.warn("Trade {} already exists with different payload, rejecting", tradeId);
                throw new ConflictException("Trade " + tradeId + " already exists with different payload");
            }
        }

        // Create new trade
        logger.debug("Creating new trade: {}", tradeId);
        Trade trade = new Trade(
                tradeId,
                request.getAccountId(),
                request.getSymbol(),
                request.getQuantity(),
                request.getPrice(),
                Trade.Side.valueOf(request.getSide()),
                System.nanoTime()
        );

        tradeMapper.insert(trade);
        logger.info("Trade {} created successfully", tradeId);

        ledgerService.generateEntries(trade);

        return convertToResponse(trade);
    }

    private boolean payloadMatches(Trade existing, CreateTradeRequest request) {
        return existing.getAccountId().equals(request.getAccountId())
                && existing.getSymbol().equals(request.getSymbol())
                && existing.getQuantity().compareTo(request.getQuantity()) == 0
                && existing.getPrice().compareTo(request.getPrice()) == 0
                && existing.getSide().name().equals(request.getSide());
    }

    private TradeResponse convertToResponse(Trade trade) {
        return new TradeResponse(
                trade.getTradeId().toString(),
                trade.getAccountId(),
                trade.getSymbol(),
                trade.getQuantity(),
                trade.getPrice(),
                trade.getSide().name(),
                trade.getTimestampNs(),
                trade.getCreatedAt()
        );
    }
}
