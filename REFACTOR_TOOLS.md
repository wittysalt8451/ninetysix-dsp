# Migratie: `tools.cpp` → `utils/`

Dit document beschrijft wat er in **sudwalfulkaan-dsp** is veranderd, zodat je firmware-projecten (Makefile / broncode) hetzelfde patroon kunt volgen.

## Wat is gedaan

- **`tools.cpp` verwijderd** — stond los in de repo-root zonder header en met **globale state** voor de envelope follower.
- Nieuwe map **`utils/`** met dezelfde namespace **`sudwalfulkaan`**, elk onderdeel in eigen **`.h` / `.cpp`**.
- **Envelope follower** is een class **`EnvelopeFollower`** met eigen state per instantie (geen globale `smooth_cv` / `smoothing_factor` meer).
- Functienamen voor mapping zijn **duidelijker** gemaakt (`MapLinear`, `MapLogarithmic`); de oude `mapping` / `logMapping` bestaan niet meer.
- **`SlewLimiter`** hernoemd naar **`SlewTowards`** om verwarring met `effects/dynamics/Limiter` te vermijden.
- Gecommentarieerde **`MakeMonoBelowFreq`** is niet meegenomen (dode code); als je die nog nodig hebt, kun je die uit git-historie terugzetten.

## Bestanden om te compileren

Voeg in je project de nieuwe bronbestanden toe en verwijder **`tools.cpp`**.

| Nieuw bronbestand |
|-------------------|
| `library/utils/Mapping.cpp` |
| `library/utils/EnvelopeFollower.cpp` |
| `library/utils/Slew.cpp` |
| `library/utils/Tempo.cpp` |

## Include-pad

Zelfde conventie als `effects/`: include directory moet de **parent** van `utils` zijn (bij symlink `library` → deze repo):

```make
CFLAGS += -Ilibrary
```

Voorbeelden:

```cpp
#include "utils/Mapping.h"
#include "utils/EnvelopeFollower.h"
#include "utils/Slew.h"
#include "utils/Tempo.h"
```

Of vanaf firmware-root:

```cpp
#include "library/utils/Mapping.h"
```

## API-wijzigingen (refactor in jouw code)

| Oud (`tools.cpp`) | Nieuw |
|-------------------|--------|
| `mapping(pot, min, max)` | `MapLinear(pot, min, max)` |
| `logMapping(pot, min, max)` | `MapLogarithmic(pot, min, max)` |
| `envelopeFollower(inL, inR)` + globale state | `EnvelopeFollower` instantie: `Init()`, `Process(inL, inR)` |
| Globale `smoothing_factor` | `SetSmoothing(...)` of `Init(smoothing)` |
| `SlewLimiter(target, current, slewRate)` | `SlewTowards(target, current, slewRate)` |
| `CalculateReleaseTime(...)` | Ongewijzigde naam; nu in `utils/Tempo.h` |

### Voorbeeld: envelope follower

**Oud:** één globale follower voor de hele app.

**Nieuw:** expliciet object (eventueel meerdere als je aparte meters nodig hebt):

```cpp
sudwalfulkaan::EnvelopeFollower envFollower;

void AudioInit() {
    envFollower.Init(0.99f);
}

float level = envFollower.Process(inL, inR);
```

## Namespace

Alles blijft in **`namespace sudwalfulkaan`**.

## Overzicht nieuwe headers

| Header | Inhoud |
|--------|--------|
| `utils/Mapping.h` | `MapLinear`, `MapLogarithmic` |
| `utils/EnvelopeFollower.h` | class `EnvelopeFollower` |
| `utils/Slew.h` | `SlewTowards` |
| `utils/Tempo.h` | `CalculateReleaseTime` |
