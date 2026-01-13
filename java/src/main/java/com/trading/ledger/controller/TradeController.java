package com.trading.ledger.controller;

import com.trading.ledger.dto.CreateTradeRequest;
import com.trading.ledger.dto.TradeResponse;
import com.trading.ledger.service.TradeCreationResult;
import com.trading.ledger.service.TradeService;
import jakarta.validation.Valid;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/trades")
public class TradeController {

    private static final Logger logger = LoggerFactory.getLogger(TradeController.class);

    private final TradeService tradeService;

    public TradeController(TradeService tradeService) {
        this.tradeService = tradeService;
    }

    /**
     * Create a new trade with idempotency guarantee.
     *
     * Behavior:
     * - First request: 201 Created
     * - Retry (same payload): 200 OK (returns existing)
     * - Retry (different payload): 409 Conflict
     */
    @PostMapping
    public ResponseEntity<TradeResponse> createTrade(@Valid @RequestBody CreateTradeRequest request) {
        logger.info("Received trade creation request: tradeId={}", request.getTradeId());

        TradeCreationResult result = tradeService.createTrade(request);

        HttpStatus status = result.isNewlyCreated() ? HttpStatus.CREATED : HttpStatus.OK;
        logger.info("Trade processed successfully: tradeId={}, status={}",
                result.getTrade().getTradeId(), status);

        return ResponseEntity.status(status).body(result.getTrade());
    }
}
