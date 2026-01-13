package com.trading.ledger;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.trading.ledger.dto.CreateTradeRequest;

import java.io.IOException;
import java.math.BigDecimal;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.Random;
import java.util.UUID;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Load generator for performance testing.
 * Generates trades at a specified rate and duration.
 */
public class LoadGenerator {

    private static final String API_BASE_URL = "http://localhost:8080/api/v1/trades";
    private static final String[] SYMBOLS = {"AAPL", "GOOGL", "MSFT", "AMZN", "TSLA", "META", "NVDA", "AMD"};
    private static final String[] SIDES = {"BUY", "SELL"};
    private static final int THREAD_COUNT = 10;

    private final HttpClient httpClient;
    private final ObjectMapper objectMapper;
    private final Random random;
    private final AtomicLong successCount;
    private final AtomicLong errorCount;

    public LoadGenerator() {
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(5))
                .build();
        this.objectMapper = new ObjectMapper();
        this.random = new Random();
        this.successCount = new AtomicLong(0);
        this.errorCount = new AtomicLong(0);
    }

    /**
     * Generate a random trade request
     */
    private CreateTradeRequest generateRandomTrade() {
        CreateTradeRequest request = new CreateTradeRequest();
        request.setTradeId(UUID.randomUUID().toString());
        request.setAccountId("ACCT-" + String.format("%06d", random.nextInt(1000)));
        request.setSymbol(SYMBOLS[random.nextInt(SYMBOLS.length)]);
        request.setQuantity(BigDecimal.valueOf(random.nextInt(1000) + 1));
        request.setPrice(BigDecimal.valueOf(100.0 + random.nextDouble() * 900.0));
        request.setSide(SIDES[random.nextInt(SIDES.length)]);
        return request;
    }

    /**
     * Submit a single trade to the API
     */
    private void submitTrade() {
        try {
            CreateTradeRequest trade = generateRandomTrade();
            String jsonBody = objectMapper.writeValueAsString(trade);

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(API_BASE_URL))
                    .header("Content-Type", "application/json")
                    .timeout(Duration.ofSeconds(5))
                    .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
                    .build();

            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());

            if (response.statusCode() == 201 || response.statusCode() == 200) {
                successCount.incrementAndGet();
            } else {
                errorCount.incrementAndGet();
                System.err.println("Error response: " + response.statusCode() + " - " + response.body());
            }
        } catch (Exception e) {
            errorCount.incrementAndGet();
            System.err.println("Exception submitting trade: " + e.getMessage());
        }
    }

    /**
     * Run the load test
     */
    public void run(int targetRate, int durationSeconds) throws InterruptedException {
        System.out.println("=== Load Generator Starting ===");
        System.out.println("Target Rate: " + targetRate + " trades/sec");
        System.out.println("Duration: " + durationSeconds + " seconds");
        System.out.println("Threads: " + THREAD_COUNT);
        System.out.println("API: " + API_BASE_URL);
        System.out.println();

        ExecutorService executor = Executors.newFixedThreadPool(THREAD_COUNT);
        ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);

        // Calculate interval between submissions (in microseconds)
        long intervalMicros = 1_000_000L / targetRate;

        // Track start time
        long startTime = System.currentTimeMillis();
        long targetSubmissions = (long) targetRate * durationSeconds;

        // Progress reporting task (every 10 seconds)
        ScheduledFuture<?> progressTask = scheduler.scheduleAtFixedRate(() -> {
            long elapsed = (System.currentTimeMillis() - startTime) / 1000;
            long current = successCount.get();
            long errors = errorCount.get();
            long actualRate = elapsed > 0 ? current / elapsed : 0;
            System.out.printf("Progress: %d seconds | Submitted: %d | Errors: %d | Rate: %d trades/sec%n",
                    elapsed, current, errors, actualRate);
        }, 10, 10, TimeUnit.SECONDS);

        // Submit trades at target rate
        System.out.println("Starting submission...");
        for (long i = 0; i < targetSubmissions; i++) {
            final long submissionNumber = i;

            // Schedule submission at precise interval
            long delayMicros = submissionNumber * intervalMicros;
            long delayMillis = delayMicros / 1000;
            long delayNanos = (delayMicros % 1000) * 1000;

            scheduler.schedule(() -> executor.execute(this::submitTrade),
                    delayMillis, TimeUnit.MILLISECONDS);
        }

        // Wait for duration + grace period for all requests to complete
        Thread.sleep(durationSeconds * 1000L + 5000L);

        // Stop progress reporting
        progressTask.cancel(true);

        // Shutdown executors
        scheduler.shutdown();
        executor.shutdown();
        executor.awaitTermination(30, TimeUnit.SECONDS);
        scheduler.awaitTermination(5, TimeUnit.SECONDS);

        // Print final statistics
        long endTime = System.currentTimeMillis();
        double actualDuration = (endTime - startTime) / 1000.0;
        long totalSubmitted = successCount.get();
        long totalErrors = errorCount.get();
        double actualRate = totalSubmitted / actualDuration;

        System.out.println();
        System.out.println("=== Load Generator Complete ===");
        System.out.println("Total: " + totalSubmitted + " trades in " + String.format("%.1f", actualDuration) + "s");
        System.out.println("Actual Rate: " + String.format("%.0f", actualRate) + " trades/sec");
        System.out.println("Success: " + totalSubmitted);
        System.out.println("Errors: " + totalErrors);
        System.out.println("Success Rate: " + String.format("%.2f%%", (totalSubmitted * 100.0 / (totalSubmitted + totalErrors))));
    }

    /**
     * Main method for command-line execution
     */
    public static void main(String[] args) {
        int rate = 10000;  // Default: 10K trades/sec
        int duration = 60;  // Default: 60 seconds

        // Parse command line arguments
        for (String arg : args) {
            if (arg.startsWith("--rate=")) {
                rate = Integer.parseInt(arg.substring(7));
            } else if (arg.startsWith("--duration=")) {
                duration = Integer.parseInt(arg.substring(11));
            }
        }

        try {
            LoadGenerator generator = new LoadGenerator();
            generator.run(rate, duration);
        } catch (Exception e) {
            System.err.println("Load generator failed: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
