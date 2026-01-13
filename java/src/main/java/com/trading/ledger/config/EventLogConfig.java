package com.trading.ledger.config;

import com.trading.ledger.eventlog.FileEventLogWriter;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;

@Configuration
public class EventLogConfig {

    @Bean
    public FileEventLogWriter fileEventLogWriter(
            @Value("${eventlog.file-path}") String filePath) throws IOException {

        Path logPath = Paths.get(filePath);

        // Ensure parent directory exists
        if (logPath.getParent() != null) {
            logPath.getParent().toFile().mkdirs();
        }

        return new FileEventLogWriter(logPath);
    }
}
