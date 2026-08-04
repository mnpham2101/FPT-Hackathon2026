// The one file an added plugin edits (D4). Adding a plugin is one new file
// under src/cra/ plus one registry.add line here — no edit to the interface,
// the store, the emitter or any existing plugin. Registration is explicit,
// never static-init: in a static library the linker discards unreferenced
// registrar objects (D4).

#include "cra/registry.hpp"

#include "config/config.hpp"

namespace ada::cra {

void registerBuiltinPlugins(Registry& /*registry*/, const config::Config& /*config*/) {
  // Empty in Phase 2: no builtin plugin exists yet. Phase 4 adds one
  // registry.add(std::make_unique<ChainedCollision>(cfg)) line per plugin.
}

}  // namespace ada::cra
