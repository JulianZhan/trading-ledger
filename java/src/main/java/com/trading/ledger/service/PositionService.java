package com.trading.ledger.service;

import com.trading.ledger.dto.PositionResponse;
import com.trading.ledger.mapper.LedgerEntryMapper;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
@RequiredArgsConstructor
@Slf4j
public class PositionService {

    private final LedgerEntryMapper ledgerEntryMapper;

    @Transactional(readOnly = true)
    public List<PositionResponse> calculatePositions(String accountId) {
        log.debug("Calculating positions for account: {}", accountId);
        List<PositionResponse> positions = ledgerEntryMapper.calculatePositionsByAccountId(accountId);
        log.debug("Found {} positions for account {}", positions.size(), accountId);
        return positions;
    }
}
