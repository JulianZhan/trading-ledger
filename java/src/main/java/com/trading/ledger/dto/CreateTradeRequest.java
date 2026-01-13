package com.trading.ledger.dto;

import jakarta.validation.constraints.*;
import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.math.BigDecimal;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class CreateTradeRequest {

    @NotNull(message = "Trade ID is required")
    private String tradeId;

    @NotBlank(message = "Account ID is required")
    @Size(max = 64, message = "Account ID must not exceed 64 characters")
    private String accountId;

    @NotBlank(message = "Symbol is required")
    @Size(max = 16, message = "Symbol must not exceed 16 characters")
    @Pattern(regexp = "^[A-Z0-9]+$", message = "Symbol must contain only uppercase letters and numbers")
    private String symbol;

    @NotNull(message = "Quantity is required")
    @DecimalMin(value = "0.00000001", message = "Quantity must be greater than 0")
    @Digits(integer = 10, fraction = 8, message = "Quantity must have at most 10 integer digits and 8 decimal digits")
    private BigDecimal quantity;

    @NotNull(message = "Price is required")
    @DecimalMin(value = "0.00000001", message = "Price must be greater than 0")
    @Digits(integer = 10, fraction = 8, message = "Price must have at most 10 integer digits and 8 decimal digits")
    private BigDecimal price;

    @NotNull(message = "Side is required")
    @Pattern(regexp = "^(BUY|SELL)$", message = "Side must be either BUY or SELL")
    private String side;
}
