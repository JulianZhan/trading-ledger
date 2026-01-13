package com.trading.ledger.service;

import com.trading.ledger.domain.Trade;
import com.trading.ledger.dto.CreateTradeRequest;
import com.trading.ledger.dto.TradeResponse;
import com.trading.ledger.eventlog.FileEventLogWriter;
import com.trading.ledger.exception.ConflictException;
import com.trading.ledger.mapper.TradeMapper;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.math.BigDecimal;
import java.util.Optional;
import java.util.UUID;

import static org.assertj.core.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

@ExtendWith(MockitoExtension.class)
class TradeServiceTest {

    @Mock
    private TradeMapper tradeMapper;

    @Mock
    private LedgerService ledgerService;

    @Mock
    private FileEventLogWriter eventLogWriter;

    @InjectMocks
    private TradeService tradeService;

    private CreateTradeRequest validRequest;
    private UUID tradeId;

    @BeforeEach
    void setUp() {
        tradeId = UUID.randomUUID();
        validRequest = new CreateTradeRequest(
                tradeId.toString(),
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "BUY"
        );
    }

    @Test
    void testCreateTrade_NewTrade_CreatesTradeAndLedgerEntries() {
        // Given - trade does not exist
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.empty());

        doNothing().when(tradeMapper).insert(any(Trade.class));

        // When
        TradeCreationResult result = tradeService.createTrade(validRequest);

        // Then
        assertThat(result).isNotNull();
        assertThat(result.isNewlyCreated()).isTrue();

        TradeResponse response = result.getTrade();
        assertThat(response).isNotNull();
        assertThat(response.getTradeId()).isEqualTo(tradeId.toString());
        assertThat(response.getAccountId()).isEqualTo("acc1");
        assertThat(response.getSymbol()).isEqualTo("AAPL");
        assertThat(response.getQuantity()).isEqualByComparingTo(BigDecimal.valueOf(100));
        assertThat(response.getPrice()).isEqualByComparingTo(BigDecimal.valueOf(150.00));
        assertThat(response.getSide()).isEqualTo("BUY");

        // Verify interactions
        verify(tradeMapper, times(1)).findByTradeId(tradeId);
        verify(tradeMapper, times(1)).insert(any(Trade.class));
        verify(ledgerService, times(1)).generateEntries(any(Trade.class));
    }

    @Test
    void testCreateTrade_DuplicateWithSamePayload_ReturnsExisting() {
        // Given - trade exists with same payload
        Trade existingTrade = new Trade(
                tradeId,
                validRequest.getAccountId(),
                validRequest.getSymbol(),
                validRequest.getQuantity(),
                validRequest.getPrice(),
                Trade.Side.BUY,
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When
        TradeCreationResult result = tradeService.createTrade(validRequest);

        // Then - should return existing trade
        assertThat(result).isNotNull();
        assertThat(result.isNewlyCreated()).isFalse();

        TradeResponse response = result.getTrade();
        assertThat(response).isNotNull();
        assertThat(response.getTradeId()).isEqualTo(tradeId.toString());
        assertThat(response.getAccountId()).isEqualTo("acc1");

        // Verify no new trade is created
        verify(tradeMapper, times(1)).findByTradeId(tradeId);
        verify(tradeMapper, never()).insert(any(Trade.class));
        verify(ledgerService, never()).generateEntries(any(Trade.class));
    }

    @Test
    void testCreateTrade_DuplicateWithDifferentAccountId_ThrowsConflict() {
        // Given - trade exists with different account_id
        Trade existingTrade = new Trade(
                tradeId,
                "acc2",  // Different account ID
                validRequest.getSymbol(),
                validRequest.getQuantity(),
                validRequest.getPrice(),
                Trade.Side.BUY,
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When/Then - should throw ConflictException
        assertThatThrownBy(() -> tradeService.createTrade(validRequest))
                .isInstanceOf(ConflictException.class)
                .hasMessageContaining("already exists with different payload");

        // Verify no new trade is created
        verify(tradeMapper, times(1)).findByTradeId(tradeId);
        verify(tradeMapper, never()).insert(any(Trade.class));
        verify(ledgerService, never()).generateEntries(any(Trade.class));
    }

    @Test
    void testCreateTrade_DuplicateWithDifferentSymbol_ThrowsConflict() {
        // Given - trade exists with different symbol
        Trade existingTrade = new Trade(
                tradeId,
                validRequest.getAccountId(),
                "MSFT",  // Different symbol
                validRequest.getQuantity(),
                validRequest.getPrice(),
                Trade.Side.BUY,
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When/Then
        assertThatThrownBy(() -> tradeService.createTrade(validRequest))
                .isInstanceOf(ConflictException.class)
                .hasMessageContaining("already exists with different payload");
    }

    @Test
    void testCreateTrade_DuplicateWithDifferentQuantity_ThrowsConflict() {
        // Given - trade exists with different quantity
        Trade existingTrade = new Trade(
                tradeId,
                validRequest.getAccountId(),
                validRequest.getSymbol(),
                BigDecimal.valueOf(200),  // Different quantity
                validRequest.getPrice(),
                Trade.Side.BUY,
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When/Then
        assertThatThrownBy(() -> tradeService.createTrade(validRequest))
                .isInstanceOf(ConflictException.class)
                .hasMessageContaining("already exists with different payload");
    }

    @Test
    void testCreateTrade_DuplicateWithDifferentPrice_ThrowsConflict() {
        // Given - trade exists with different price
        Trade existingTrade = new Trade(
                tradeId,
                validRequest.getAccountId(),
                validRequest.getSymbol(),
                validRequest.getQuantity(),
                BigDecimal.valueOf(200.00),  // Different price
                Trade.Side.BUY,
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When/Then
        assertThatThrownBy(() -> tradeService.createTrade(validRequest))
                .isInstanceOf(ConflictException.class)
                .hasMessageContaining("already exists with different payload");
    }

    @Test
    void testCreateTrade_DuplicateWithDifferentSide_ThrowsConflict() {
        // Given - trade exists with different side
        Trade existingTrade = new Trade(
                tradeId,
                validRequest.getAccountId(),
                validRequest.getSymbol(),
                validRequest.getQuantity(),
                validRequest.getPrice(),
                Trade.Side.SELL,  // Different side
                System.nanoTime()
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.of(existingTrade));

        // When/Then
        assertThatThrownBy(() -> tradeService.createTrade(validRequest))
                .isInstanceOf(ConflictException.class)
                .hasMessageContaining("already exists with different payload");
    }

    @Test
    void testCreateTrade_SellTrade_CreatesSuccessfully() {
        // Given - SELL trade
        CreateTradeRequest sellRequest = new CreateTradeRequest(
                tradeId.toString(),
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "SELL"
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.empty());

        doNothing().when(tradeMapper).insert(any(Trade.class));

        // When
        TradeCreationResult result = tradeService.createTrade(sellRequest);

        // Then
        assertThat(result).isNotNull();
        assertThat(result.isNewlyCreated()).isTrue();
        assertThat(result.getTrade().getSide()).isEqualTo("SELL");
        verify(tradeMapper, times(1)).insert(any(Trade.class));
        verify(ledgerService, times(1)).generateEntries(any(Trade.class));
    }

    @Test
    void testCreateTrade_DecimalQuantityAndPrice_HandledCorrectly() {
        // Given - decimal values
        CreateTradeRequest decimalRequest = new CreateTradeRequest(
                tradeId.toString(),
                "acc1",
                "BTC",
                new BigDecimal("0.12345678"),
                new BigDecimal("45000.12345678"),
                "BUY"
        );
        when(tradeMapper.findByTradeId(tradeId)).thenReturn(Optional.empty());

        Trade savedTrade = new Trade(
                tradeId,
                decimalRequest.getAccountId(),
                decimalRequest.getSymbol(),
                decimalRequest.getQuantity(),
                decimalRequest.getPrice(),
                Trade.Side.BUY,
                System.nanoTime()
        );
        doNothing().when(tradeMapper).insert(any(Trade.class));

        // When
        TradeCreationResult result = tradeService.createTrade(decimalRequest);

        // Then
        assertThat(result).isNotNull();
        assertThat(result.isNewlyCreated()).isTrue();
        assertThat(result.getTrade().getQuantity()).isEqualByComparingTo(new BigDecimal("0.12345678"));
        assertThat(result.getTrade().getPrice()).isEqualByComparingTo(new BigDecimal("45000.12345678"));
    }
}
