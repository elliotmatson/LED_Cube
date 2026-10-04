/**
 * @file waiting_stream.h
 * @brief A Stream view of a network client that waits for data instead of
 * reporting the end of it.
 */
#ifndef WAITING_STREAM_H
#define WAITING_STREAM_H

#include <Arduino.h>

/**
 * @brief Wraps a network client so a parser reading from it waits through
 * gaps in the data.
 *
 * NetworkClientSecure::read(buf, size) returns -1 whenever no decrypted data
 * is ready yet, and NetworkClient::readBytes() treats -1 as an error and
 * stops. ArduinoJson reads through readBytes(), so the first pause in a
 * download -- every time, on a busy network -- ended the input and the parse
 * failed with IncompleteInput. This class overrides only the single-byte
 * read(), so Stream's own readBytes() is used: it retries until the client's
 * timeout passes with nothing arriving.
 */
class WaitingStream : public Stream
{
public:
    /** @param source the client to read from; its timeout is used. */
    explicit WaitingStream(Stream &source) : source(source)
    {
        setTimeout(source.getTimeout());
    }

    int available() override { return source.available(); }
    int read() override { return source.read(); }
    int peek() override { return source.peek(); }
    size_t write(uint8_t) override { return 0; }

private:
    Stream &source;
};

#endif
