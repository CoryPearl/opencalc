# Third-Party Notices

OpenCalc-authored software is Copyright (C) 2026 Cory Pearl and is licensed
under the GNU General Public License, version 3 or, at your option, any later
version. Third-party components retain their original copyright and license
terms. The top-level GPL license does not replace those notices.

## Eigenmath

`firmware/main/components/eigenmath/` vendors and modifies Eigenmath from
<https://github.com/georgeweigt/eigenmath>.

Copyright (c) 2024 George Weigt. Licensed under BSD-2-Clause. The complete
upstream notice is preserved in
`firmware/main/components/eigenmath/LICENSE` and in the source.

## Giac / KhiCAS

`firmware/components/giac/` vendors a Giac 1.4.9/KhiCAS-derived embedded
snapshot adapted for ESP32-S3. It was imported from NeoCalculator commit
`823ac86c4fafc32c55ce0f17584992ddf953b43f`; the closest documented upstream
baseline is `KhiCAS/ti-ce-giac` commit
`8d24f392f3edcb4fbf44b11325e92ca37edee470`.

Giac is Copyright (C) Bernard Parisse and contributors. Giac, the embedded
port, and OpenCalc's integration are distributed under GPL-3.0-or-later.
Detailed provenance is preserved in
`firmware/components/giac/NUMOS_CHANGES.md` and
`firmware/components/giac/OPENCALC_INTEGRATION.md`.

`firmware/components/libtommath/` vendors LibTomMath snapshot
`652d70a31fce`; its upstream source is released under the Unlicense. Local
integration changes retain their source notices.

## Doom and doomgeneric

`firmware/main/components/doomgeneric/` is derived from the Doom source,
Chocolate Doom, and <https://github.com/ozkl/doomgeneric>.

Relevant source files preserve copyright notices for id Software, Simon
Howard, Raven Software, and other contributors. Those files state GNU GPL
version 2 or, at the recipient's option, a later version. Their notices remain
authoritative.

## Anemoia-ESP32

`firmware/main/components/mario/` is derived from
<https://github.com/Shim06/Anemoia-ESP32> by Shim06 and contributors. Upstream
distributes Anemoia-ESP32 under GNU GPL version 3. OpenCalc modifications and
the combined firmware are distributed under compatible GPL terms without
removing or replacing upstream copyright.

## Espressif and TinyUSB

OpenCalc builds on Espressif ESP-IDF and managed components including
Espressif's TinyUSB integration, TinyUSB, and the ESP LCD ILI9341 component.
These dependencies retain the copyright notices and licenses shipped in their
source distributions and generated `managed_components/` directories.

## Game data

Game data is legally separate from the GPL-licensed firmware:

- `doom1.wad` is id Software's Doom shareware data and remains subject to its
  shareware distribution terms. Registered or commercial Doom WADs are not
  distributed under the GPL.
- NES ROM files, including `mario.nes`, remain copyrighted by their respective
  owners and are not covered by OpenCalc's license. Distributors and users are
  responsible for supplying only ROM images they are legally permitted to use
  and redistribute.

No OpenCalc license grant applies to third-party trademarks, game artwork,
music, ROMs, WADs, or other proprietary assets.
