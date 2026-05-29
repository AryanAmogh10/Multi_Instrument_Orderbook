#include "ordbk/protocol/framer.hpp"

namespace ordbk::protocol
{

void Framer::feed(std::span<const std::byte> data)
{
    if (error_ != Error::None)
        return;
    buf_.insert(buf_.end(), data.begin(), data.end());
}

std::optional<DecodedMessage> Framer::next()
{
    if (error_ != Error::None)
        return std::nullopt;
    if (buf_.size() < kHeaderSize)
        return std::nullopt;

    BufferReader hr{std::span<const std::byte>{buf_.data(), kHeaderSize}};
    auto header = decode_header(hr);
    if (!header)
        return std::nullopt; // unreachable: header read can't fail at this point

    if (header->protocol_version != kProtocolVersion)
    {
        error_ = Error::BadVersion;
        return std::nullopt;
    }
    if (header->body_length > kMaxBodySize)
    {
        error_ = Error::OversizeBody;
        return std::nullopt;
    }

    const std::size_t total = kHeaderSize + header->body_length;
    if (buf_.size() < total)
        return std::nullopt; // need more bytes

    auto body_span = std::span<const std::byte>{buf_.data() + kHeaderSize, header->body_length};
    auto body = decode_body(*header, body_span);
    if (!body)
    {
        error_ = Error::BadBody;
        return std::nullopt;
    }

    DecodedMessage out{*header, std::move(*body)};
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(total));
    return out;
}

} // namespace ordbk::protocol
