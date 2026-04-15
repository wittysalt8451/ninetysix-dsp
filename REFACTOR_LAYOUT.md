# Migratie: library-indeling (envelopes, control, generators)

Dit document vormt een aanvulling op [REFACTOR_TOOLS.md](REFACTOR_TOOLS.md) en beschrijft de **mapstructuur** die is doorgevoerd om `ui/` en `effects/` inhoudelijk scherper te trekken.

## Wat is gedaan

| Oud pad | Nieuw pad |
|---------|-----------|
| `effects/envelopes/Envelope.h` / `.cpp` | `envelopes/Envelope.h` / `.cpp` |
| `ui/ClockDetector.h` / `.cpp` | `control/ClockDetector.h` / `.cpp` |
| `ui/GatePulse.h` / `.cpp` | `control/GatePulse.h` / `.cpp` |

- **`generators/`** is toegevoegd met **`.gitkeep`** (lege map voor toekomstige oscillators, LFO’s, enz.).
- **`effects/envelopes/`** is verwijderd; ADSR hoort niet onder “audio effects” maar onder modulatie/hulpenvelopes.

Includes en namespace zijn ongewijzigd qua **API** (`class Envelope`, `ClockDetector`, `GatePulse`), alleen paden naar bestanden zijn anders.

## Makefile / bronlijst

Verwijder oude bronregels en voeg toe:

| Toevoegen |
|-----------|
| `library/envelopes/Envelope.cpp` |
| `library/control/ClockDetector.cpp` |
| `library/control/GatePulse.cpp` |

Verwijder:

| Verwijderen |
|-------------|
| `library/effects/envelopes/Envelope.cpp` |
| `library/ui/ClockDetector.cpp` |
| `library/ui/GatePulse.cpp` |

## Includes in jouw firmware

| Oud | Nieuw |
|-----|--------|
| `#include "envelopes/Envelope.h"` met `-Ilibrary/effects` | `#include "envelopes/Envelope.h"` met `-Ilibrary` (parent van `envelopes/`) |
| `#include "library/effects/envelopes/Envelope.h"` | `#include "library/envelopes/Envelope.h"` |
| `#include "ui/ClockDetector.h"` | `#include "control/ClockDetector.h"` |
| `#include "ui/GatePulse.h"` | `#include "control/GatePulse.h"` |

Zorg dat **`CFLAGS` `-Ilibrary`** bevat (zoals in [README.md](README.md)); dan werken `envelopes/...`, `control/...`, `ui/...`, `utils/...` allemaal met het korte pad.

## UI-map

`ui/` bevat nu vooral **UIManager** en **ParamSmoother** (mens-interface / pot-frontend). Clock- en gate-logica zit onder **`control/`**.
