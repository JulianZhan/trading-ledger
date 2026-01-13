package com.trading.ledger.controller;

import com.trading.ledger.dto.PositionResponse;
import com.trading.ledger.service.PositionService;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/positions")
@RequiredArgsConstructor
@Slf4j
public class PositionController {

    private final PositionService positionService;

    @GetMapping("/{accountId}")
    public ResponseEntity<List<PositionResponse>> getPositions(@PathVariable String accountId) {
        log.info("GET /api/v1/positions/{}", accountId);
        List<PositionResponse> positions = positionService.calculatePositions(accountId);
        return ResponseEntity.ok(positions);
    }
}
