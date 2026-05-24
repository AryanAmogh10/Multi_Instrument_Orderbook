#pragma once

#include "velox/protocol/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace velox::protocol {

// Encoding errors are signalled by short return values; decoding errors return
// nullopt. The wire format is little-endian throughout (roadmap §3.1).

class BufferWriter {
public:
    explicit BufferWriter(std::span<std::byte> buf) noexcept : buf_(buf) {}

    [[nodiscard]] bool write_u8(std::uint8_t v) noexcept;
    [[nodiscard]] bool write_u16(std::uint16_t v) noexcept;
    [[nodiscard]] bool write_u32(std::uint32_t v) noexcept;
    [[nodiscard]] bool write_u64(std::uint64_t v) noexcept;
    [[nodiscard]] bool write_i64(std::int64_t v) noexcept;

    [[nodiscard]] std::size_t pos() const noexcept { return pos_; }

private:
    std::span<std::byte> buf_;
    std::size_t pos_{0};
};

class BufferReader {
public:
    explicit BufferReader(std::span<const std::byte> buf) noexcept : buf_(buf) {}

    [[nodiscard]] bool read_u8(std::uint8_t& out) noexcept;
    [[nodiscard]] bool read_u16(std::uint16_t& out) noexcept;
    [[nodiscard]] bool read_u32(std::uint32_t& out) noexcept;
    [[nodiscard]] bool read_u64(std::uint64_t& out) noexcept;
    [[nodiscard]] bool read_i64(std::int64_t& out) noexcept;

    [[nodiscard]] std::size_t pos()       const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return buf_.size() - pos_; }

private:
    std::span<const std::byte> buf_;
    std::size_t pos_{0};
};

// ---- Header --------------------------------------------------------------
[[nodiscard]] bool encode_header(BufferWriter&, const MessageHeader&) noexcept;
[[nodiscard]] std::optional<MessageHeader> decode_header(BufferReader&) noexcept;

// ---- Whole-message encode (header + body) -------------------------------
// Returns bytes written, or 0 on overflow.
[[nodiscard]] std::size_t encode(std::span<std::byte> out, std::uint32_t seq, const MessageBody&);

// Convenience: encode into a fresh vector.
[[nodiscard]] std::vector<std::byte> encode(std::uint32_t seq, const MessageBody&);

// ---- Body decode given a header ----------------------------------------
[[nodiscard]] std::optional<MessageBody>
decode_body(const MessageHeader& header, std::span<const std::byte> body) noexcept;

}  // namespace velox::protocol
