package com.trading.ledger.controller;

import com.trading.ledger.domain.LedgerEntry;
import com.trading.ledger.domain.Trade;
import com.trading.ledger.dto.LedgerEntryResponse;
import com.trading.ledger.dto.TradeResponse;
import com.trading.ledger.mapper.LedgerEntryMapper;
import com.trading.ledger.mapper.TradeMapper;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.stream.Collectors;

@RestController
@RequestMapping("/api/v1")
@RequiredArgsConstructor
@Slf4j
public class LedgerController {

    private final LedgerEntryMapper ledgerEntryMapper;
    private final TradeMapper tradeMapper;

    @GetMapping("/ledger/entries")
    public ResponseEntity<List<LedgerEntryResponse>> getLedgerEntries(
            @RequestParam(required = false) String tradeId,
            @RequestParam(required = false) String accountId,
            @RequestParam(defaultValue = "20") int limit,
            @RequestParam(defaultValue = "0") int offset) {

        log.info("GET /api/v1/ledger/entries?tradeId={}&accountId={}&limit={}&offset={}",
                tradeId, accountId, limit, offset);

        List<LedgerEntry> entries;
        if (tradeId != null) {
            entries = ledgerEntryMapper.findByTradeId(tradeId);
        } else if (accountId != null) {
            entries = ledgerEntryMapper.findByAccountId(accountId, limit, offset);
        } else {
            return ResponseEntity.badRequest().build();
        }

        List<LedgerEntryResponse> responses = entries.stream()
                .map(LedgerEntryResponse::from)
                .collect(Collectors.toList());

        return ResponseEntity.ok(responses);
    }

    @GetMapping("/trades")
    public ResponseEntity<List<TradeResponse>> getTrades(
            @RequestParam String accountId,
            @RequestParam(defaultValue = "20") int limit,
            @RequestParam(defaultValue = "0") int offset) {

        log.info("GET /api/v1/trades?accountId={}&limit={}&offset={}", accountId, limit, offset);

        List<Trade> trades = tradeMapper.findByAccountId(accountId, limit, offset);
        List<TradeResponse> responses = trades.stream()
                .map(TradeResponse::from)
                .collect(Collectors.toList());

        return ResponseEntity.ok(responses);
    }
}
