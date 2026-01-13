package com.trading.ledger.mapper;

import com.trading.ledger.domain.Trade;
import org.apache.ibatis.annotations.Mapper;
import org.apache.ibatis.annotations.Param;

import java.util.List;
import java.util.Optional;

@Mapper
public interface TradeMapper {

    Optional<Trade> findByTradeId(@Param("tradeId") String tradeId);

    void insert(Trade trade);

    List<Trade> findByAccountId(@Param("accountId") String accountId,
                                  @Param("limit") int limit,
                                  @Param("offset") int offset);
}
