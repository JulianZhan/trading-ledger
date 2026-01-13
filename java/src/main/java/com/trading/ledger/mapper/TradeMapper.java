package com.trading.ledger.mapper;

import com.trading.ledger.domain.Trade;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.util.Optional;
import java.util.UUID;

@Mapper
public interface TradeMapper {

    Optional<Trade> findByTradeId(@Param("tradeId") UUID tradeId);

    void insert(Trade trade);
}
