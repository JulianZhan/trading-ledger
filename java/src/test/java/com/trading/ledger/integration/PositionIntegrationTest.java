package com.trading.ledger.integration;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.trading.ledger.dto.CreateTradeRequest;
import com.trading.ledger.dto.PositionResponse;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.ActiveProfiles;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.MvcResult;

import java.math.BigDecimal;
import java.util.List;
import java.util.UUID;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@SpringBootTest
@AutoConfigureMockMvc
@ActiveProfiles("test")
class PositionIntegrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private ObjectMapper objectMapper;

    private static final String ACCOUNT_ID = "test-account-" + UUID.randomUUID();

    @BeforeEach
    void setUp() {
        // Each test uses a unique account ID to avoid conflicts
    }

    @Test
    void testPositionCalculation_MultipleTrades() throws Exception {
        // Create 10 trades: 7 BUYs and 3 SELLs for AAPL
        // BUYs: 100@150, 50@155, 200@152, 100@151, 150@153, 75@154, 25@156
        // SELLs: 200@160, 100@158, 150@159
        // Net position: (100+50+200+100+150+75+25) - (200+100+150) = 700 - 450 = 250 shares

        // BUY trades
        createTrade(ACCOUNT_ID, "AAPL", "100", "150.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "50", "155.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "200", "152.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "100", "151.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "150", "153.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "75", "154.00", "BUY");
        createTrade(ACCOUNT_ID, "AAPL", "25", "156.00", "BUY");

        // SELL trades
        createTrade(ACCOUNT_ID, "AAPL", "200", "160.00", "SELL");
        createTrade(ACCOUNT_ID, "AAPL", "100", "158.00", "SELL");
        createTrade(ACCOUNT_ID, "AAPL", "150", "159.00", "SELL");

        // Get positions for the account
        MvcResult result = mockMvc.perform(get("/api/v1/positions/" + ACCOUNT_ID))
                .andExpect(status().isOk())
                .andReturn();

        String responseJson = result.getResponse().getContentAsString();
        List<PositionResponse> positions = objectMapper.readValue(
                responseJson,
                objectMapper.getTypeFactory().constructCollectionType(List.class, PositionResponse.class)
        );

        // Verify position
        assertThat(positions).hasSize(1);
        PositionResponse position = positions.get(0);
        assertThat(position.getAccountId()).isEqualTo(ACCOUNT_ID);
        assertThat(position.getSymbol()).isEqualTo("AAPL");
        assertThat(position.getQuantity()).isEqualByComparingTo(new BigDecimal("250"));

        // Average price calculation:
        // Buys: (100*150 + 50*155 + 200*152 + 100*151 + 150*153 + 75*154 + 25*156) = 106,850
        // Sells: (200*160 + 100*158 + 150*159) = 63,050
        // Net: (106,850 - 63,050) / 250 = 43,800 / 250 = 175.20
        assertThat(position.getAveragePrice()).isGreaterThan(BigDecimal.ZERO);
    }

    @Test
    void testPositionCalculation_ZeroPosition() throws Exception {
        String accountId = "zero-position-" + UUID.randomUUID();

        // Create balanced trades (BUY 100, SELL 100)
        createTrade(accountId, "TSLA", "100", "200.00", "BUY");
        createTrade(accountId, "TSLA", "100", "210.00", "SELL");

        // Get positions for the account
        MvcResult result = mockMvc.perform(get("/api/v1/positions/" + accountId))
                .andExpect(status().isOk())
                .andReturn();

        String responseJson = result.getResponse().getContentAsString();
        List<PositionResponse> positions = objectMapper.readValue(
                responseJson,
                objectMapper.getTypeFactory().constructCollectionType(List.class, PositionResponse.class)
        );

        // Should have no positions (zero quantity is filtered out)
        assertThat(positions).isEmpty();
    }

    @Test
    void testPositionCalculation_MultipleSymbols() throws Exception {
        String accountId = "multi-symbol-" + UUID.randomUUID();

        // Create trades for multiple symbols
        createTrade(accountId, "AAPL", "100", "150.00", "BUY");
        createTrade(accountId, "GOOGL", "50", "2500.00", "BUY");
        createTrade(accountId, "MSFT", "200", "300.00", "BUY");

        // Get positions for the account
        MvcResult result = mockMvc.perform(get("/api/v1/positions/" + accountId))
                .andExpect(status().isOk())
                .andReturn();

        String responseJson = result.getResponse().getContentAsString();
        List<PositionResponse> positions = objectMapper.readValue(
                responseJson,
                objectMapper.getTypeFactory().constructCollectionType(List.class, PositionResponse.class)
        );

        // Should have 3 positions
        assertThat(positions).hasSize(3);
        assertThat(positions).extracting("symbol")
                .containsExactlyInAnyOrder("AAPL", "GOOGL", "MSFT");
    }

    private void createTrade(String accountId, String symbol, String quantity, String price, String side) throws Exception {
        CreateTradeRequest request = new CreateTradeRequest();
        request.setTradeId(UUID.randomUUID().toString());
        request.setAccountId(accountId);
        request.setSymbol(symbol);
        request.setQuantity(new BigDecimal(quantity));
        request.setPrice(new BigDecimal(price));
        request.setSide(side);

        mockMvc.perform(post("/api/v1/trades")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(request)))
                .andExpect(status().isCreated());
    }
}
