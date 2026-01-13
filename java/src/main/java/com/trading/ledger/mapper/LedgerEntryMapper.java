package com.trading.ledger.mapper;

import com.trading.ledger.domain.LedgerEntry;
import com.trading.ledger.dto.PositionResponse;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.math.BigDecimal;
import java.util.List;

@Mapper
public interface LedgerEntryMapper {

    BigDecimal sumEntriesByTradeId(@Param("tradeId") String tradeId);

    void insertAll(@Param("entries") List<LedgerEntry> entries);

    List<LedgerEntry> findByTradeId(@Param("tradeId") String tradeId);

    List<LedgerEntry> findByAccountId(@Param("accountId") String accountId,
                                       @Param("limit") int limit,
                                       @Param("offset") int offset);

    List<PositionResponse> calculatePositionsByAccountId(@Param("accountId") String accountId);
}
