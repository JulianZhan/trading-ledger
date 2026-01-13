package com.trading.ledger.service;

import com.trading.ledger.domain.LedgerEntry;
import com.trading.ledger.domain.Trade;
import com.trading.ledger.mapper.LedgerEntryMapper;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

import static org.assertj.core.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

@ExtendWith(MockitoExtension.class)
class LedgerServiceTest {

    @Mock
    private LedgerEntryMapper ledgerEntryMapper;

    @InjectMocks
    private LedgerService ledgerService;

    @Captor
    private ArgumentCaptor<List<LedgerEntry>> entriesCaptor;

    private Trade sampleTrade;
    private UUID tradeId;

    @BeforeEach
    void setUp() {
        tradeId = UUID.randomUUID();
        sampleTrade = new Trade(
                tradeId,
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                Trade.Side.BUY,
                System.nanoTime()
        );
    }

    @Test
    void testGenerateEntries_CreatesTwoEntriesDebitAndCredit() {
        // Given
        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        List<LedgerEntry> entries = ledgerService.generateEntries(sampleTrade);

        // Then
        assertThat(entries).hasSize(2);
        verify(ledgerEntryMapper, times(1)).insertAll(entriesCaptor.capture());

        List<LedgerEntry> capturedEntries = entriesCaptor.getValue();
        assertThat(capturedEntries).hasSize(2);

        // Verify DEBIT entry
        LedgerEntry debit = capturedEntries.stream()
                .filter(e -> e.getEntryType() == LedgerEntry.EntryType.DEBIT)
                .findFirst()
                .orElseThrow();
        assertThat(debit.getTradeId()).isEqualTo(tradeId);
        assertThat(debit.getAccountId()).isEqualTo("acc1");
        assertThat(debit.getAmount()).isEqualByComparingTo(BigDecimal.valueOf(15000));

        // Verify CREDIT entry (stored as positive; query handles negation)
        LedgerEntry credit = capturedEntries.stream()
                .filter(e -> e.getEntryType() == LedgerEntry.EntryType.CREDIT)
                .findFirst()
                .orElseThrow();
        assertThat(credit.getTradeId()).isEqualTo(tradeId);
        assertThat(credit.getAccountId()).isEqualTo("acc1");
        assertThat(credit.getAmount()).isEqualByComparingTo(BigDecimal.valueOf(15000));
    }

    @Test
    void testGenerateEntries_EntriesSumToZero_ValidationPasses() {
        // Given - entries that sum to zero
        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        List<LedgerEntry> entries = ledgerService.generateEntries(sampleTrade);

        // Then - should not throw exception
        assertThat(entries).isNotNull();
        verify(ledgerEntryMapper, times(1)).sumEntriesByTradeId(tradeId);
    }

    @Test
    void testGenerateEntries_EntriesDoNotSumToZero_ThrowsException() {
        // Given - entries that do NOT sum to zero (invariant violation)
        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.valueOf(100));  // Should be 0!

        // When/Then - should throw IllegalStateException
        assertThatThrownBy(() -> ledgerService.generateEntries(sampleTrade))
                .isInstanceOf(IllegalStateException.class)
                .hasMessageContaining("Double-entry accounting invariant violated")
                .hasMessageContaining("sum = 100");
    }

    @Test
    void testGenerateEntries_CalculatesCorrectAmount() {
        // Given - trade with quantity=100, price=150.00
        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        ledgerService.generateEntries(sampleTrade);

        // Then - verify amount calculation (100 * 150.00 = 15000)
        verify(ledgerEntryMapper, times(1)).insertAll(entriesCaptor.capture());
        List<LedgerEntry> entries = entriesCaptor.getValue();

        BigDecimal totalAmount = entries.stream()
                .map(LedgerEntry::getAmount)
                .map(BigDecimal::abs)
                .findFirst()
                .orElseThrow();

        assertThat(totalAmount).isEqualByComparingTo(
                sampleTrade.getQuantity().multiply(sampleTrade.getPrice())
        );
    }

    @Test
    void testGenerateEntries_SellTrade_GeneratesCorrectEntries() {
        // Given - SELL trade
        Trade sellTrade = new Trade(
                tradeId,
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                Trade.Side.SELL,
                System.nanoTime()
        );

        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        List<LedgerEntry> entries = ledgerService.generateEntries(sellTrade);

        // Then - should still create 2 entries that balance
        assertThat(entries).hasSize(2);
        verify(ledgerEntryMapper, times(1)).insertAll(any(List.class));
        verify(ledgerEntryMapper, times(1)).sumEntriesByTradeId(tradeId);
    }

    @Test
    void testGenerateEntries_DecimalQuantityAndPrice_HandlesCorrectly() {
        // Given - decimal values (e.g., crypto trading)
        Trade cryptoTrade = new Trade(
                tradeId,
                "acc1",
                "BTC",
                new BigDecimal("0.12345678"),
                new BigDecimal("45000.12345678"),
                Trade.Side.BUY,
                System.nanoTime()
        );

        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        ledgerService.generateEntries(cryptoTrade);

        // Then - verify correct calculation without rounding errors
        verify(ledgerEntryMapper, times(1)).insertAll(entriesCaptor.capture());
        List<LedgerEntry> entries = entriesCaptor.getValue();

        BigDecimal expectedAmount = new BigDecimal("0.12345678")
                .multiply(new BigDecimal("45000.12345678"));

        LedgerEntry debit = entries.stream()
                .filter(e -> e.getEntryType() == LedgerEntry.EntryType.DEBIT)
                .findFirst()
                .orElseThrow();

        assertThat(debit.getAmount()).isEqualByComparingTo(expectedAmount);
    }

    @Test
    void testGenerateEntries_PreservesTradeMetadata() {
        // Given
        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        ledgerService.generateEntries(sampleTrade);

        // Then - verify entries have correct metadata
        verify(ledgerEntryMapper, times(1)).insertAll(entriesCaptor.capture());
        List<LedgerEntry> entries = entriesCaptor.getValue();

        for (LedgerEntry entry : entries) {
            assertThat(entry.getTradeId()).isEqualTo(tradeId);
            assertThat(entry.getAccountId()).isEqualTo("acc1");
            assertThat(entry.getTimestampNs()).isEqualTo(sampleTrade.getTimestampNs());
        }
    }

    @Test
    void testGenerateEntries_SmallAmount_NoRoundingErrors() {
        // Given - very small amounts
        Trade smallTrade = new Trade(
                tradeId,
                "acc1",
                "BTC",
                new BigDecimal("0.00000001"),
                new BigDecimal("0.00000001"),
                Trade.Side.BUY,
                System.nanoTime()
        );

        doNothing().when(ledgerEntryMapper).insertAll(any(List.class));
        when(ledgerEntryMapper.sumEntriesByTradeId(tradeId))
                .thenReturn(BigDecimal.ZERO);

        // When
        ledgerService.generateEntries(smallTrade);

        // Then - should handle without precision loss
        verify(ledgerEntryMapper, times(1)).insertAll(entriesCaptor.capture());
        List<LedgerEntry> entries = entriesCaptor.getValue();

        BigDecimal debitAmount = entries.stream()
                .filter(e -> e.getEntryType() == LedgerEntry.EntryType.DEBIT)
                .findFirst()
                .map(LedgerEntry::getAmount)
                .orElseThrow();

        BigDecimal creditAmount = entries.stream()
                .filter(e -> e.getEntryType() == LedgerEntry.EntryType.CREDIT)
                .findFirst()
                .map(LedgerEntry::getAmount)
                .orElseThrow();

        // Verify they are equal (both positive; query handles negation for balance check)
        assertThat(debitAmount).isEqualByComparingTo(creditAmount);
    }
}
