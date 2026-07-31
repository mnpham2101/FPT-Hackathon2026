// V2X_ECU/src/codec/vanetza_cpm_codec.cpp — VanetzaCpmCodec: CpmContent ⇄ ASN.1 UPER
// over vanetza::asn1::r2::Cpm. Structure and field mapping are fixed by the frozen
// profile contracts/r1-cpm-profile.md §2–§3; nothing here derives, converts or clamps.
//
// Memory discipline: every sub-allocation attached to the message tree comes from
// vanetza::asn1::allocate<T>() (calloc-backed), so the wrapper's descriptor-driven
// ASN_STRUCT_FREE in ~asn1c_wrapper_common matches. Elements handed to
// ASN_SEQUENCE_ADD are held in a descriptor-aware unique_ptr until the add succeeds,
// so a failure on any path frees them exactly once.

#include "codec/vanetza_cpm_codec.hpp"

#include <memory>
#include <stdexcept>
#include <string>

#include <vanetza/asn1/asn1c_wrapper.hpp>
#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/support/asn_SEQUENCE_OF.h>

namespace v2x::codec {
namespace {

// --- Profile constants (contracts/r1-cpm-profile.md §2) ---------------------------
//
// ItsPduHeader.protocolVersion and PerceivedObjectContainer.numberOfPerceivedObjects
// are plain constrained INTEGERs in the CDD (OrdinalNumber1B / CardinalNumber1B), so
// asn1c generates no named value for them; the profile fixes both.
constexpr long kProtocolVersion = 2;            // ITS release 2 PDU header
constexpr long kNumberOfPerceivedObjects = 1;   // exactly one perceived object (C)

// F9: DeltaTimeMilliSecondSigned spans −2048..2047 on the wire, but the profile
// narrows it to ±2047 — −2048 is wire-encodable yet profile-invalid.
constexpr int kMeasurementDeltaTimeAbsMax = 2047;

// Descriptor-aware owning pointer for elements built before ASN_SEQUENCE_ADD.
template <class T>
using OwnedNode = std::unique_ptr<T, vanetza::asn1::deleter>;

// --- Encode -----------------------------------------------------------------------

void FillHeader(Vanetza_ITS2_ItsPduHeader_t& header, const CpmContent& content) {
  header.protocolVersion = kProtocolVersion;
  header.messageId = Vanetza_ITS2_MessageId_cpm;  // cpm(14)
  header.stationId = content.stationId;
}

void FillManagementContainer(Vanetza_ITS2_CPM_PDU_Descriptions_ManagementContainer_t& mgmt,
                             const CpmContent& content) {
  // TimestampIts is generated as INTEGER_t (arbitrary precision), not a native long,
  // so it must be written through the asn1c conversion helper.
  if (asn_uint642INTEGER(&mgmt.referenceTime, content.referenceTime) != 0) {
    throw std::runtime_error("VanetzaCpmCodec: cannot encode referenceTime as TimestampIts");
  }

  mgmt.referencePosition.latitude = content.referencePosition.latitude;
  mgmt.referencePosition.longitude = content.referencePosition.longitude;

  // Unsent-mandatory (profile §3): PosConfidenceEllipse and Altitude are not carried
  // by CpmContent, so each component takes its CDD "unavailable" named value.
  mgmt.referencePosition.positionConfidenceEllipse.semiMajorConfidence =
      Vanetza_ITS2_SemiAxisLength_unavailable;  // 4095
  mgmt.referencePosition.positionConfidenceEllipse.semiMinorConfidence =
      Vanetza_ITS2_SemiAxisLength_unavailable;  // 4095
  mgmt.referencePosition.positionConfidenceEllipse.semiMajorOrientation =
      Vanetza_ITS2_HeadingValue_unavailable;  // 3601
  mgmt.referencePosition.altitude.altitudeValue =
      Vanetza_ITS2_AltitudeValue_unavailable;  // 800001
  mgmt.referencePosition.altitude.altitudeConfidence =
      Vanetza_ITS2_AltitudeConfidence_unavailable;  // 15

  // segmentationInfo and messageRateRange are OPTIONAL and unsent — left NULL.
}

// Builds container [id=1] OriginatingVehicleContainer, carrying orientationAngle.
OwnedNode<Vanetza_ITS2_WrappedCpmContainer_t> MakeOriginatingVehicleContainer(
    const CpmContent& content) {
  auto wrapper = vanetza::asn1::make_unique<Vanetza_ITS2_WrappedCpmContainer_t>(
      asn_DEF_Vanetza_ITS2_WrappedCpmContainer);

  wrapper->containerId = Vanetza_ITS2_CpmContainerId_originatingVehicleContainer;  // 1
  wrapper->containerData.present =
      Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingVehicleContainer;

  auto& ovc = wrapper->containerData.choice.OriginatingVehicleContainer;
  ovc.orientationAngle.value = content.orientationAngle;
  // Unsent-mandatory (profile §3): the Wgs84Angle confidence component.
  ovc.orientationAngle.confidence = Vanetza_ITS2_Wgs84AngleConfidence_unavailable;  // 127
  // pitchAngle, rollAngle, trailerDataSet are OPTIONAL and unsent — left NULL.

  return wrapper;
}

// Builds the single PerceivedObject describing C.
OwnedNode<Vanetza_ITS2_PerceivedObject_t> MakePerceivedObject(const CpmPerceivedObject& object) {
  auto po = vanetza::asn1::make_unique<Vanetza_ITS2_PerceivedObject_t>(
      asn_DEF_Vanetza_ITS2_PerceivedObject);

  // objectId is OPTIONAL in the CDD but always sent by the profile.
  po->objectId = vanetza::asn1::allocate<Vanetza_ITS2_Identifier2B_t>();
  *po->objectId = object.objectId;

  po->measurementDeltaTime = object.measurementDeltaTime;

  po->position.xCoordinate.value = object.position.x;
  po->position.xCoordinate.confidence = object.position.xConfidence;
  po->position.yCoordinate.value = object.position.y;
  po->position.yCoordinate.confidence = object.position.yConfidence;
  // zCoordinate is OPTIONAL and unsent — left NULL.

  // velocity is OPTIONAL in the CDD; the profile always sends the cartesian arm.
  po->velocity = vanetza::asn1::allocate<Vanetza_ITS2_Velocity3dWithConfidence_t>();
  po->velocity->present = Vanetza_ITS2_Velocity3dWithConfidence_PR_cartesianVelocity;
  auto& cartesian = po->velocity->choice.cartesianVelocity;
  cartesian.xVelocity.value = object.velocity.x;
  cartesian.yVelocity.value = object.velocity.y;
  // Unsent-mandatory (profile §3): the VelocityComponent confidence (SpeedConfidence).
  cartesian.xVelocity.confidence = Vanetza_ITS2_SpeedConfidence_unavailable;  // 127
  cartesian.yVelocity.confidence = Vanetza_ITS2_SpeedConfidence_unavailable;  // 127
  // zVelocity is OPTIONAL and unsent — left NULL.

  // classification is OPTIONAL in the CDD; the profile always sends exactly one entry.
  po->classification = vanetza::asn1::allocate<Vanetza_ITS2_ObjectClassDescription_t>();
  auto klass = vanetza::asn1::make_unique<Vanetza_ITS2_ObjectClassWithConfidence_t>(
      asn_DEF_Vanetza_ITS2_ObjectClassWithConfidence);
  klass->objectClass.present = Vanetza_ITS2_ObjectClass_PR_vehicleSubClass;
  klass->objectClass.choice.vehicleSubClass = object.classification;  // M1: passengerCar(5)
  klass->confidence = object.classConfidence;
  if (ASN_SEQUENCE_ADD(&po->classification->list, klass.get()) != 0) {
    throw std::runtime_error("VanetzaCpmCodec: cannot append object classification");
  }
  klass.release();  // now owned by po->classification->list

  // The remaining 12 OPTIONAL PerceivedObject fields stay NULL (profile §2).
  return po;
}

// Builds container [id=5] PerceivedObjectContainer, carrying exactly one object.
OwnedNode<Vanetza_ITS2_WrappedCpmContainer_t> MakePerceivedObjectContainer(
    const CpmContent& content) {
  auto wrapper = vanetza::asn1::make_unique<Vanetza_ITS2_WrappedCpmContainer_t>(
      asn_DEF_Vanetza_ITS2_WrappedCpmContainer);

  wrapper->containerId = Vanetza_ITS2_CpmContainerId_perceivedObjectContainer;  // 5
  wrapper->containerData.present =
      Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer;

  auto& poc = wrapper->containerData.choice.PerceivedObjectContainer;
  // Unsent-mandatory (profile §3): fixed at 1 and ignored on decode.
  poc.numberOfPerceivedObjects = kNumberOfPerceivedObjects;

  auto po = MakePerceivedObject(content.object);
  if (ASN_SEQUENCE_ADD(&poc.perceivedObjects.list, po.get()) != 0) {
    throw std::runtime_error("VanetzaCpmCodec: cannot append perceived object");
  }
  po.release();  // now owned by poc.perceivedObjects.list

  return wrapper;
}

// --- Decode -----------------------------------------------------------------------

const Vanetza_ITS2_WrappedCpmContainer_t* FindContainer(
    const Vanetza_ITS2_ConstraintWrappedCpmContainers_t& containers, long container_id) {
  for (int i = 0; i < containers.list.count; ++i) {
    const auto* candidate = containers.list.array[i];
    if (candidate != nullptr && candidate->containerId == container_id) {
      return candidate;
    }
  }
  return nullptr;
}

}  // namespace

std::vector<std::uint8_t> VanetzaCpmCodec::encode(const CpmContent& content) const {
  // F9 gate first, before anything is allocated.
  if (content.object.measurementDeltaTime > kMeasurementDeltaTimeAbsMax ||
      content.object.measurementDeltaTime < -kMeasurementDeltaTimeAbsMax) {
    throw std::out_of_range(
        "VanetzaCpmCodec: measurementDeltaTime " +
        std::to_string(content.object.measurementDeltaTime) +
        " ms violates the F9 profile bound of +/-" +
        std::to_string(kMeasurementDeltaTimeAbsMax) + " ms");
  }

  // Everything below hangs off this wrapper, whose destructor deep-frees the tree —
  // including on the throwing paths.
  vanetza::asn1::r2::Cpm cpm;
  auto& message = *cpm;

  FillHeader(message.header, content);
  FillManagementContainer(message.payload.managementContainer, content);

  auto originating = MakeOriginatingVehicleContainer(content);
  if (ASN_SEQUENCE_ADD(&message.payload.cpmContainers.list, originating.get()) != 0) {
    throw std::runtime_error("VanetzaCpmCodec: cannot append OriginatingVehicleContainer");
  }
  originating.release();  // now owned by the message tree

  auto perceived = MakePerceivedObjectContainer(content);
  if (ASN_SEQUENCE_ADD(&message.payload.cpmContainers.list, perceived.get()) != 0) {
    throw std::runtime_error("VanetzaCpmCodec: cannot append PerceivedObjectContainer");
  }
  perceived.release();  // now owned by the message tree

  // encode() throws std::runtime_error if UPER encoding fails (e.g. a CpmContent
  // value outside its CDD range), so no partial buffer can escape.
  return cpm.encode();
}

std::variant<CpmContent, DecodeError> VanetzaCpmCodec::decode(const std::uint8_t* data,
                                                              std::size_t len) const {
  if (data == nullptr || len == 0) {
    return DecodeError{"empty CPM buffer"};
  }

  try {
    vanetza::asn1::r2::Cpm cpm;
    if (!cpm.decode(data, len)) {
      return DecodeError{"UPER decoding of CollectivePerceptionMessage failed"};
    }
    const auto& message = *cpm;

    CpmContent content;
    // header.protocolVersion and header.messageId are ignored on decode (profile §3).
    content.stationId = static_cast<std::uint32_t>(message.header.stationId);

    const auto& mgmt = message.payload.managementContainer;
    std::uint64_t reference_time = 0;
    if (asn_INTEGER2uint64(&mgmt.referenceTime, &reference_time) != 0) {
      return DecodeError{"referenceTime is not representable as an unsigned 64-bit value"};
    }
    content.referenceTime = reference_time;
    content.referencePosition.latitude = static_cast<std::int32_t>(mgmt.referencePosition.latitude);
    content.referencePosition.longitude =
        static_cast<std::int32_t>(mgmt.referencePosition.longitude);
    // positionConfidenceEllipse and altitude are ignored on decode (profile §3).

    const auto* originating =
        FindContainer(message.payload.cpmContainers,
                      Vanetza_ITS2_CpmContainerId_originatingVehicleContainer);
    if (originating == nullptr ||
        originating->containerData.present !=
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingVehicleContainer) {
      return DecodeError{"CPM carries no OriginatingVehicleContainer (id=1)"};
    }
    content.orientationAngle = static_cast<std::uint16_t>(
        originating->containerData.choice.OriginatingVehicleContainer.orientationAngle.value);

    const auto* perceived = FindContainer(message.payload.cpmContainers,
                                          Vanetza_ITS2_CpmContainerId_perceivedObjectContainer);
    if (perceived == nullptr ||
        perceived->containerData.present !=
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer) {
      return DecodeError{"CPM carries no PerceivedObjectContainer (id=5)"};
    }
    // numberOfPerceivedObjects is ignored on decode (profile §3); the list drives it.
    const auto& objects =
        perceived->containerData.choice.PerceivedObjectContainer.perceivedObjects;
    if (objects.list.count < 1 || objects.list.array[0] == nullptr) {
      return DecodeError{"PerceivedObjectContainer carries no perceived object"};
    }
    const auto& object = *objects.list.array[0];

    if (object.objectId == nullptr) {
      return DecodeError{"perceived object carries no objectId"};
    }
    content.object.objectId = static_cast<std::uint16_t>(*object.objectId);
    content.object.measurementDeltaTime =
        static_cast<std::int16_t>(object.measurementDeltaTime);

    content.object.position.x = static_cast<std::int32_t>(object.position.xCoordinate.value);
    content.object.position.xConfidence =
        static_cast<std::uint16_t>(object.position.xCoordinate.confidence);
    content.object.position.y = static_cast<std::int32_t>(object.position.yCoordinate.value);
    content.object.position.yConfidence =
        static_cast<std::uint16_t>(object.position.yCoordinate.confidence);

    if (object.velocity == nullptr ||
        object.velocity->present != Vanetza_ITS2_Velocity3dWithConfidence_PR_cartesianVelocity) {
      return DecodeError{"perceived object carries no cartesian velocity"};
    }
    const auto& cartesian = object.velocity->choice.cartesianVelocity;
    content.object.velocity.x = static_cast<std::int16_t>(cartesian.xVelocity.value);
    content.object.velocity.y = static_cast<std::int16_t>(cartesian.yVelocity.value);
    // The VelocityComponent confidences are ignored on decode (profile §3).

    if (object.classification == nullptr || object.classification->list.count < 1 ||
        object.classification->list.array[0] == nullptr) {
      return DecodeError{"perceived object carries no classification"};
    }
    const auto& klass = *object.classification->list.array[0];
    if (klass.objectClass.present != Vanetza_ITS2_ObjectClass_PR_vehicleSubClass) {
      return DecodeError{"perceived object classification is not a vehicle sub-class"};
    }
    content.object.classification =
        static_cast<std::uint8_t>(klass.objectClass.choice.vehicleSubClass);
    content.object.classConfidence = static_cast<std::uint8_t>(klass.confidence);

    return content;
  } catch (const std::exception& error) {
    return DecodeError{std::string("CPM decoding raised: ") + error.what()};
  } catch (...) {
    return DecodeError{"CPM decoding raised an unknown error"};
  }
}

}  // namespace v2x::codec
