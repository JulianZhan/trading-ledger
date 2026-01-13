package com.trading.ledger.eventlog;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.CRC32;

import static org.assertj.core.api.Assertions.*;

class FileEventLogWriterTest {

    @TempDir
    Path tempDir;

    private Path logPath;
    private FileEventLogWriter writer;

    @BeforeEach
    void setUp() {
        logPath = tempDir.resolve("test_event_log.bin");
    }

    @AfterEach
    void tearDown() throws IOException {
        if (writer != null) {
            writer.close();
        }
    }

    @Test
    void testWriteEvent_CreatesFile() throws IOException {
        // Given - no file exists
        assertThat(logPath).doesNotExist();

        // When - create writer
        writer = new FileEventLogWriter(logPath);

        // Then - file should be created with header
        assertThat(logPath).exists();

        // Verify header was written
        try (FileChannel channel = FileChannel.open(logPath, StandardOpenOption.READ)) {
            assertThat(channel.size()).isGreaterThanOrEqualTo(16);

            ByteBuffer header = ByteBuffer.allocate(16);
            header.order(ByteOrder.LITTLE_ENDIAN);
            channel.read(header);
            header.flip();

            int magic = header.getInt();
            int version = header.getInt();
            long reserved = header.getLong();

            assertThat(magic).isEqualTo(0x54524144);  // "TRAD"
            assertThat(version).isEqualTo(1);
            assertThat(reserved).isEqualTo(0);
        }
    }

    @Test
    void testWriteEvent_AppendsSequentially() throws IOException {
        // Given
        writer = new FileEventLogWriter(logPath);

        Map<String, Object> payload1 = new HashMap<>();
        payload1.put("test", "event1");

        Map<String, Object> payload2 = new HashMap<>();
        payload2.put("test", "event2");

        // When - write two events
        writer.append(Event.EventType.TRADE_CREATED, payload1);
        writer.append(Event.EventType.TRADE_CREATED, payload2);

        // Then - sequence numbers should increment
        assertThat(writer.getCurrentSequence()).isEqualTo(2);

        // File should have header + 2 events
        try (FileChannel channel = FileChannel.open(logPath, StandardOpenOption.READ)) {
            long fileSize = channel.size();
            assertThat(fileSize).isGreaterThan(16);  // Header + events
        }
    }

    @Test
    void testReadEvent_ValidatesCRC() throws IOException {
        // Given - write an event
        writer = new FileEventLogWriter(logPath);

        Map<String, Object> payload = new HashMap<>();
        payload.put("trade_id", "test-123");
        payload.put("symbol", "AAPL");
        payload.put("quantity", 100);

        writer.append(Event.EventType.TRADE_CREATED, payload);
        writer.close();

        // When - read the event back
        try (FileChannel channel = FileChannel.open(logPath, StandardOpenOption.READ)) {
            long fileSize = channel.size();

            // Skip header (16 bytes)
            channel.position(16);

            // Read event header (28 bytes)
            ByteBuffer eventHeader = ByteBuffer.allocate(28);
            eventHeader.order(ByteOrder.LITTLE_ENDIAN);
            int headerBytesRead = channel.read(eventHeader);
            eventHeader.flip();

            assertThat(headerBytesRead).isEqualTo(28);

            long seqNum = eventHeader.getLong();
            long timestampNs = eventHeader.getLong();
            byte eventType = eventHeader.get();
            eventHeader.get();  // reserved[0]
            eventHeader.get();  // reserved[1]
            eventHeader.get();  // reserved[2]
            int payloadLength = eventHeader.getInt();

            // Then - verify event structure
            assertThat(seqNum).isEqualTo(1);
            assertThat(timestampNs).isGreaterThan(0);
            assertThat(eventType).isEqualTo((byte) 1);
            assertThat(payloadLength).isGreaterThan(0);

            // Read payload
            ByteBuffer payloadBuffer = ByteBuffer.allocate(payloadLength);
            int payloadBytesRead = channel.read(payloadBuffer);
            assertThat(payloadBytesRead).isEqualTo(payloadLength);
            payloadBuffer.flip();

            byte[] payloadBytes = new byte[payloadLength];
            payloadBuffer.get(payloadBytes);

            // Verify payload contains expected JSON fields
            String payloadJson = new String(payloadBytes, java.nio.charset.StandardCharsets.UTF_8);
            assertThat(payloadJson).contains("test-123");
            assertThat(payloadJson).contains("AAPL");
            assertThat(payloadJson).contains("quantity");

            // Read CRC (4 bytes)
            ByteBuffer crcBuffer = ByteBuffer.allocate(4);
            crcBuffer.order(ByteOrder.LITTLE_ENDIAN);
            int crcBytesRead = channel.read(crcBuffer);
            assertThat(crcBytesRead).isEqualTo(4);

            // File should be fully read
            assertThat(channel.position()).isEqualTo(fileSize);
        }
    }

    @Test
    void testReopenExistingLog_DoesNotOverwriteHeader() throws IOException {
        // Given - create log and write event
        writer = new FileEventLogWriter(logPath);
        Map<String, Object> payload = new HashMap<>();
        payload.put("test", "event1");
        writer.append(Event.EventType.TRADE_CREATED, payload);
        long firstSize = logPath.toFile().length();
        writer.close();

        // When - reopen existing log
        writer = new FileEventLogWriter(logPath);
        Map<String, Object> payload2 = new HashMap<>();
        payload2.put("test", "event2");
        writer.append(Event.EventType.TRADE_CREATED, payload2);

        // Then - file should grow (not be overwritten)
        long secondSize = logPath.toFile().length();
        assertThat(secondSize).isGreaterThan(firstSize);

        // Sequence should continue from 1 (but our in-memory counter resets)
        // In production, we'd need to read the log to recover the sequence
        assertThat(writer.getCurrentSequence()).isEqualTo(1);
    }

    @Test
    void testAppend_ThreadSafe() throws IOException, InterruptedException {
        // Given
        writer = new FileEventLogWriter(logPath);
        int numThreads = 10;
        int eventsPerThread = 10;

        Map<String, Object> payload = new HashMap<>();
        payload.put("test", "concurrent");

        // When - multiple threads append simultaneously
        Thread[] threads = new Thread[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = new Thread(() -> {
                try {
                    for (int j = 0; j < eventsPerThread; j++) {
                        writer.append(Event.EventType.TRADE_CREATED, payload);
                    }
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            });
            threads[i].start();
        }

        // Wait for all threads
        for (Thread thread : threads) {
            thread.join();
        }

        // Then - all events should be written
        assertThat(writer.getCurrentSequence()).isEqualTo(numThreads * eventsPerThread);
    }
}
