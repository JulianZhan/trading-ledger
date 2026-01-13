#pragma once

#include "Event.h"
#include <string>
#include <fstream>
#include <memory>

namespace trading_ledger {

class FileReader {
public:
    explicit FileReader(const std::string& file_path);
    ~FileReader();

    // Open file and validate file header
    void open();

    // Read next event from file
    // Returns true if event was read, false if EOF
    bool readNext(Event& event);

    // Get current file offset
    size_t offset() const { return offset_; }

    // Check if EOF reached
    bool eof() const;

private:
    std::string file_path_;
    std::ifstream file_;
    size_t offset_;
    FileHeader file_header_;
};

}  // namespace trading_ledger
