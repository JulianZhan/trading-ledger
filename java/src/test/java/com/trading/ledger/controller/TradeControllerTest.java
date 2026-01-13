package com.trading.ledger.controller;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.trading.ledger.dto.CreateTradeRequest;
import com.trading.ledger.dto.TradeResponse;
import com.trading.ledger.exception.ConflictException;
import com.trading.ledger.service.TradeCreationResult;
import com.trading.ledger.service.TradeService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.context.ActiveProfiles;
import org.springframework.test.web.servlet.MockMvc;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

import static org.hamcrest.Matchers.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(TradeController.class)
@ActiveProfiles("test")
class TradeControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private ObjectMapper objectMapper;

    @MockBean
    private TradeService tradeService;

    @Test
    void testCreateTrade_ValidRequest_Returns201() throws Exception {
        // Given
        String tradeId = UUID.randomUUID().toString();
        CreateTradeRequest request = new CreateTradeRequest(
                tradeId,
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "BUY"
        );

        TradeResponse mockResponse = new TradeResponse(
                tradeId,
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "BUY",
                System.nanoTime(),
                Instant.now()
        );

        when(tradeService.createTrade(any(CreateTradeRequest.class)))
                .thenReturn(new TradeCreationResult(mockResponse, true));

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.tradeId").value(tradeId))
                .andExpect(jsonPath("$.accountId").value("acc1"))
                .andExpect(jsonPath("$.symbol").value("AAPL"))
                .andExpect(jsonPath("$.quantity").value(100))
                .andExpect(jsonPath("$.price").value(150.00))
                .andExpect(jsonPath("$.side").value("BUY"))
                .andExpect(jsonPath("$.timestampNs").isNumber())
                .andExpect(jsonPath("$.createdAt").isNotEmpty());
    }

    @Test
    void testCreateTrade_MissingTradeId_Returns400() throws Exception {
        // Given - missing tradeId
        String json = """
                {
                    "accountId": "acc1",
                    "symbol": "AAPL",
                    "quantity": 100,
                    "price": 150.00,
                    "side": "BUY"
                }
                """;

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(json))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.error").value("Validation Failed"))
                .andExpect(jsonPath("$.errors.tradeId").value("Trade ID is required"));
    }

    @Test
    void testCreateTrade_MissingAccountId_Returns400() throws Exception {
        // Given - missing accountId
        String json = """
                {
                    "tradeId": "%s",
                    "symbol": "AAPL",
                    "quantity": 100,
                    "price": 150.00,
                    "side": "BUY"
                }
                """.formatted(UUID.randomUUID().toString());

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(json))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.errors.accountId").value("Account ID is required"));
    }

    @Test
    void testCreateTrade_InvalidSymbol_Returns400() throws Exception {
        // Given - symbol with lowercase letters
        CreateTradeRequest request = new CreateTradeRequest(
                UUID.randomUUID().toString(),
                "acc1",
                "aapl",  // Invalid: should be uppercase
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "BUY"
        );

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.errors.symbol").value("Symbol must contain only uppercase letters and numbers"));
    }

    @Test
    void testCreateTrade_NegativeQuantity_Returns400() throws Exception {
        // Given - negative quantity
        CreateTradeRequest request = new CreateTradeRequest(
                UUID.randomUUID().toString(),
                "acc1",
                "AAPL",
                BigDecimal.valueOf(-100),  // Invalid: negative
                BigDecimal.valueOf(150.00),
                "BUY"
        );

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.errors.quantity").value("Quantity must be greater than 0"));
    }

    @Test
    void testCreateTrade_ZeroPrice_Returns400() throws Exception {
        // Given - zero price
        CreateTradeRequest request = new CreateTradeRequest(
                UUID.randomUUID().toString(),
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.ZERO,  // Invalid: zero
                "BUY"
        );

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.errors.price").value("Price must be greater than 0"));
    }

    @Test
    void testCreateTrade_InvalidSide_Returns400() throws Exception {
        // Given - invalid side
        CreateTradeRequest request = new CreateTradeRequest(
                UUID.randomUUID().toString(),
                "acc1",
                "AAPL",
                BigDecimal.valueOf(100),
                BigDecimal.valueOf(150.00),
                "HOLD"  // Invalid: must be BUY or SELL
        );

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.errors.side").value("Side must be either BUY or SELL"));
    }

    @Test
    void testCreateTrade_MissingAllFields_Returns400WithMultipleErrors() throws Exception {
        // Given - empty request
        String json = "{}";

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(json))
                .andExpect(status().isBadRequest())
                .andExpect(jsonPath("$.status").value(400))
                .andExpect(jsonPath("$.error").value("Validation Failed"))
                .andExpect(jsonPath("$.errors").isMap())
                .andExpect(jsonPath("$.errors", aMapWithSize(greaterThanOrEqualTo(5))));  // At least 5 validation errors
    }

    @Test
    void testCreateTrade_SellSide_Returns201() throws Exception {
        // Given - SELL trade
        String tradeId = UUID.randomUUID().toString();
        CreateTradeRequest request = new CreateTradeRequest(
                tradeId,
                "acc1",
                "MSFT",
                BigDecimal.valueOf(200),
                BigDecimal.valueOf(300.50),
                "SELL"
        );

        TradeResponse mockResponse = new TradeResponse(
                tradeId,
                "acc1",
                "MSFT",
                BigDecimal.valueOf(200),
                BigDecimal.valueOf(300.50),
                "SELL",
                System.nanoTime(),
                Instant.now()
        );

        when(tradeService.createTrade(any(CreateTradeRequest.class)))
                .thenReturn(new TradeCreationResult(mockResponse, true));

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.tradeId").value(tradeId))
                .andExpect(jsonPath("$.side").value("SELL"));
    }

    @Test
    void testCreateTrade_DecimalValues_Returns201() throws Exception {
        // Given - decimal quantity and price
        String tradeId = UUID.randomUUID().toString();
        CreateTradeRequest request = new CreateTradeRequest(
                tradeId,
                "acc1",
                "BTC",
                new BigDecimal("0.12345678"),  // 8 decimal places
                new BigDecimal("45000.12345678"),  // 8 decimal places
                "BUY"
        );

        TradeResponse mockResponse = new TradeResponse(
                tradeId,
                "acc1",
                "BTC",
                new BigDecimal("0.12345678"),
                new BigDecimal("45000.12345678"),
                "BUY",
                System.nanoTime(),
                Instant.now()
        );

        when(tradeService.createTrade(any(CreateTradeRequest.class)))
                .thenReturn(new TradeCreationResult(mockResponse, true));

        // When/Then
        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.quantity").value(0.12345678))
                .andExpect(jsonPath("$.price").value(45000.12345678));
    }
}
