# Shared Vanetza pin fragment - MASTER copy (contracts/vanetza-pin.cmake).
# Single home of the Vanetza FetchContent pin and its ASN.1-only option set, synced
# byte-identical to V2X_ECU/cmake/vanetza-pin.cmake and (later)
# Scenario_Player/codec_helper/cmake/vanetza-pin.cmake under contracts/sync-manifest.json -
# edit only this master, never a synced copy in place.
# The fragment only declares; each consumer calls FetchContent_MakeAvailable itself.
# Consumers need CMake >= 3.28: FetchContent_Declare(... EXCLUDE_FROM_ALL) exists from 3.28 on.

# Idempotent, so the fragment stays self-sufficient for consumers that have not
# included the FetchContent module themselves.
include(FetchContent)

FetchContent_Declare(vanetza
  GIT_REPOSITORY https://github.com/riebl/vanetza.git
  GIT_TAG fb6c551030dcc12b924299bf401e35e5fe814713  # tag v26.06
  EXCLUDE_FROM_ALL
)

# ASN.1-only: no security/geodesy/RPC backends (HLD section 8 - no GN/BTP full-stack pull-in).
# Vanetza defaults these options to "was the package found on this machine", which would make the
# configure step depend on the runner image; forcing them keeps it deterministic.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_GEOGRAPHICLIB OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_RPC OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_CRYPTOPP OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_OPENSSL OFF CACHE BOOL "" FORCE)

# The ASN.1-only link set: Vanetza::asn1 carries the asn1c wrapper + release-1 asn1_its;
# the release-2 type descriptors (asn_DEF_Vanetza_ITS2_*) live in Vanetza::asn1_its_r2.
# The rest of the Vanetza stack stays EXCLUDE_FROM_ALL and is never linked.
set(VANETZA_ASN1_TARGETS Vanetza::asn1 Vanetza::asn1_its_r2)
