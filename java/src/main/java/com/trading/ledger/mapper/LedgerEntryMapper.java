package com.trading.ledger.mapper;

import com.trading.ledger.domain.LedgerEntry;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

@Mapper
public interface LedgerEntryMapper {

    BigDecimal sumEntriesByTradeId(@Param("tradeId") UUID tradeId);

    void insertAll(@Param("entries") List<LedgerEntry> entries);
}
