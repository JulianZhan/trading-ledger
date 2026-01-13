package com.trading.ledger.eventlog;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.ToString;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.zip.CRC32;

/**
 * Immutable event data class representing a single event in the binary event log.
 *
 * Events are serialized to binary format:
 * - 28-byte header (sequence, timestamp, type, reserved, payload length)
 * - Variable-length JSON payload (UTF-8)
 * - 4-byte CRC32 checksum
 *
 * Thread-safe due to immutability.
 */
@Getter
@AllArgsConstructor
@ToString
public class Event {

    public enum EventType {
        TRADE_CREATED((byte) 1);

        private final byte value;

        EventType(byte value) {
            this.value = value;
        }

        public byte getValue() {
            return value;
        }
    }

    private final long sequenceNum;
    private final long timestampNs;
    private final EventType eventType;
    private final String payload;

    private static final ObjectMapper objectMapper = new ObjectMapper();

    /**
     * Create event with automatic payload serialization.
     *
     * @param sequenceNum Monotonically increasing sequence number
     * @param timestampNs Nanosecond timestamp (System.nanoTime())
     * @param eventType Type of event (e.g., TRADE_CREATED)
     * @param payloadObject Object to serialize as JSON payload
     */
    public Event(long sequenceNum, long timestampNs, EventType eventType, Object payloadObject) {
        this.sequenceNum = sequenceNum;
        this.timestampNs = timestampNs;
        this.eventType = eventType;
        this.payload = serializePayload(payloadObject);
    }

    private String serializePayload(Object payloadObject) {
        try {
            return objectMapper.writeValueAsString(payloadObject);
        } catch (JsonProcessingException e) {
            throw new RuntimeException("Failed to serialize payload", e);
        }
    }

    /**
     * Serialize event to binary format (little-endian).
     *
     * Format:
     * - sequence_num (8 bytes, long)
     * - timestamp_ns (8 bytes, long)
     * - event_type (1 byte)
     * - reserved (3 bytes, padding)
     * - payload_length (4 bytes, int)
     * - payload (N bytes, UTF-8 JSON)
     * - crc32 (4 bytes, checksum over all preceding bytes)
     *
     * @return Binary representation of event
     */
    public byte[] serialize() {
        byte[] payloadBytes = payload.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        int payloadLength = payloadBytes.length;
        int totalSize = 28 + payloadLength + 4;  // 28 (header) + payload + 4 (CRC)

        ByteBuffer buffer = ByteBuffer.allocate(totalSize);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        // Write event header (28 bytes)
        buffer.putLong(sequenceNum);
        buffer.putLong(timestampNs);
        buffer.put(eventType.getValue());
        buffer.put((byte) 0);  // reserved[0]
        buffer.put((byte) 0);  // reserved[1]
        buffer.put((byte) 0);  // reserved[2]
        buffer.putInt(payloadLength);

        // Write payload
        buffer.put(payloadBytes);

        // Calculate and write CRC32 checksum
        CRC32 crc = new CRC32();
        crc.update(buffer.array(), 0, totalSize - 4);  // Exclude CRC field itself
        long crcValue = crc.getValue();
        buffer.putInt((int) crcValue);

        return buffer.array();
    }
}
