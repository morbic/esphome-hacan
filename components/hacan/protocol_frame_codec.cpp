#include "protocol_frame_codec.h"

namespace hacan::protocol {
namespace {

template <MessageKind Kind>
std::variant<ProtocolFrame, DecodeError> decode_payload(
    const CanIdentifier& identifier, const std::array<std::uint8_t, 8>& bytes) {
  const auto payload = WirePayload<Kind>::decode(bytes);
  const auto result = payload->validate(identifier);
  if (result == FrameValidationError::kInvalidPriority) return DecodeError::kInvalidPriority;
  if (result == FrameValidationError::kInvalidAddressing) return DecodeError::kInvalidAddressing;
  if (result != FrameValidationError::kNone) return DecodeError::kInvalidPayload;
  return ProtocolFrame{identifier, *payload};
}

std::variant<ProtocolFrame, DecodeError> decode_known(
    const CanIdentifier& id, const std::array<std::uint8_t, 8>& bytes) {
  switch (id.kind()) {
    case MessageKind::kProtocolError: return decode_payload<MessageKind::kProtocolError>(id, bytes);
    case MessageKind::kAnnounce: return decode_payload<MessageKind::kAnnounce>(id, bytes);
    case MessageKind::kHeartbeat: return decode_payload<MessageKind::kHeartbeat>(id, bytes);
    case MessageKind::kDiscoveryRequest: return decode_payload<MessageKind::kDiscoveryRequest>(id, bytes);
    case MessageKind::kDiscoveryResponse: return decode_payload<MessageKind::kDiscoveryResponse>(id, bytes);
    case MessageKind::kAddressClaim: return decode_payload<MessageKind::kAddressClaim>(id, bytes);
    case MessageKind::kAddressAssign: return decode_payload<MessageKind::kAddressAssign>(id, bytes);
    case MessageKind::kAddressConflict: return decode_payload<MessageKind::kAddressConflict>(id, bytes);
    case MessageKind::kState: return decode_payload<MessageKind::kState>(id, bytes);
    case MessageKind::kEvent: return decode_payload<MessageKind::kEvent>(id, bytes);
    case MessageKind::kCommand: return decode_payload<MessageKind::kCommand>(id, bytes);
    case MessageKind::kRequest: return decode_payload<MessageKind::kRequest>(id, bytes);
    case MessageKind::kResponse: return decode_payload<MessageKind::kResponse>(id, bytes);
    case MessageKind::kAck: return decode_payload<MessageKind::kAck>(id, bytes);
    case MessageKind::kConfigGet: return decode_payload<MessageKind::kConfigGet>(id, bytes);
    case MessageKind::kConfigSet: return decode_payload<MessageKind::kConfigSet>(id, bytes);
    case MessageKind::kConfigValue: return decode_payload<MessageKind::kConfigValue>(id, bytes);
    case MessageKind::kDiagnosticRequest: return decode_payload<MessageKind::kDiagnosticRequest>(id, bytes);
    case MessageKind::kDiagnosticResponse: return decode_payload<MessageKind::kDiagnosticResponse>(id, bytes);
    case MessageKind::kTimeSync: return decode_payload<MessageKind::kTimeSync>(id, bytes);
    case MessageKind::kEntityClaim: return decode_payload<MessageKind::kEntityClaim>(id, bytes);
    case MessageKind::kEntityResolve: return decode_payload<MessageKind::kEntityResolve>(id, bytes);
  }
  return DecodeError::kUnknownKind;
}

bool known_kind(MessageKind kind) {
  return static_cast<std::uint8_t>(kind) <= static_cast<std::uint8_t>(MessageKind::kEntityResolve);
}
}  // namespace

std::variant<ProtocolFrame, DecodeError> FrameCodec::decode(const RawCanFrame& raw) {
  if (!raw.extended) return DecodeError::kNotExtended;
  if (raw.dlc != 8) return DecodeError::kInvalidDlc;
  const auto identifier = CanIdentifier::from_raw(raw.can_id);
  if (!identifier) return DecodeError::kInvalidIdentifier;
  if (!known_kind(identifier->kind())) return DecodeError::kUnknownKind;
  return decode_known(*identifier, raw.data);
}

std::variant<RawCanFrame, EncodeError> FrameCodec::encode(const ProtocolFrame& frame) {
  return std::visit([&frame](const auto& payload) -> std::variant<RawCanFrame, EncodeError> {
    if (payload.validate(frame.identifier()) != FrameValidationError::kNone) {
      return EncodeError::kInvalidFrame;
    }
    return RawCanFrame{frame.identifier().to_raw(), true, 8, payload.encode()};
  }, frame.payload());
}

}  // namespace hacan::protocol
