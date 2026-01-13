package com.trading.ledger.eventlog;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.concurrent.atomic.AtomicLong;

public class FileEventLogWriter implements AutoCloseable {

    private static final Logger logger = LoggerFactory.getLogger(FileEventLogWriter.class);

    private static final int MAGIC = 0x54524144;  // "TRAD"
    private static final int VERSION = 1;
    private static final int HEADER_SIZE = 16;

    private final FileChannel channel;
    private final AtomicLong sequenceCounter;
    private final Path logPath;

    public FileEventLogWriter(Path logPath) throws IOException {
        this.logPath = logPath;
        this.sequenceCounter = new AtomicLong(0);

        // Open file in append mode, create if doesn't exist
        this.channel = FileChannel.open(logPath,
                StandardOpenOption.WRITE,
                StandardOpenOption.APPEND,
                StandardOpenOption.CREATE);

        // Write header if file is new
        if (channel.size() == 0) {
            writeHeader();
            logger.info("Created new event log at: {}", logPath);
        } else {
            logger.info("Opened existing event log at: {}", logPath);
        }
    }

    private void writeHeader() throws IOException {
        ByteBuffer header = ByteBuffer.allocate(HEADER_SIZE);
        header.order(ByteOrder.LITTLE_ENDIAN);

        header.putInt(MAGIC);
        header.putInt(VERSION);
        header.putLong(0);  // reserved

        header.flip();
        channel.write(header);
        logger.debug("Wrote event log header: magic=0x{}, version={}",
                Integer.toHexString(MAGIC), VERSION);
    }

    public synchronized void append(Event.EventType eventType, Object payload) throws IOException {
        long seqNum = sequenceCounter.incrementAndGet();
        long timestampNs = System.nanoTime();

        Event event = new Event(seqNum, timestampNs, eventType, payload);
        byte[] eventBytes = event.serialize();

        ByteBuffer buffer = ByteBuffer.wrap(eventBytes);
        int bytesWritten = channel.write(buffer);

        logger.debug("Appended event: seq={}, type={}, size={} bytes",
                seqNum, eventType, bytesWritten);
    }

    public long getCurrentSequence() {
        return sequenceCounter.get();
    }

    @Override
    public void close() throws IOException {
        if (channel != null && channel.isOpen()) {
            channel.close();
            logger.info("Closed event log: {}", logPath);
        }
    }
}
