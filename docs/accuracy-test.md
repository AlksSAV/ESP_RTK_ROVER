# Accuracy Test — Characterising the Antenna Phase-Centre Offset

The rover reports σ values of a few millimetres. Those describe the RTK engine's confidence in
its own solution; they say nothing about where the antenna's electrical phase centre actually
sits relative to the hardware you measure to. This dome has no ANTEX calibration, so that
offset is unknown — see
[README → Antenna phase centre](../README.md#antenna-phase-centre--the-uncharacterised-part).

This document is the plan to measure it.

- [What this test can and cannot resolve](#what-this-test-can-and-cannot-resolve)
- [Candidate control points](#candidate-control-points)
- [Prerequisites](#prerequisites)
- [Test A — absolute height (finds the offset)](#test-a--absolute-height-finds-the-offset)
- [Test B — height difference (validates the setup)](#test-b--height-difference-validates-the-setup)
- [Reduction](#reduction)
- [Sanity assertions](#sanity-assertions)
- [Interpreting the result](#interpreting-the-result)
- [Results](#results)
- [Data sources and attribution](#data-sources-and-attribution)

---

## What this test can and cannot resolve

Control points in Baden-Württemberg publish **normal heights in DHHN2016 (NHN)**. GNSS measures
**ellipsoidal height**. Converting between them requires the GCG2016 quasigeoid, whose stated
accuracy is **~1 cm in flatland and ~2 cm in mountains**.

That model uncertainty is the floor of this test. It is larger than the numbers the receiver
reports about itself.

| Error source | Magnitude | Behaviour |
|---|---|---|
| GCG2016 quasigeoid model | **~1–2 cm** | Systematic, dominates |
| Height transfer from mark to antenna (staff + level) | ~2–3 mm | Random, negligible here |
| RTK vertical solution, fixed, settled | ~10–15 mm | Mostly random, averages down over a session |
| Antenna phase-centre offset | **unknown — the quantity being measured** | Systematic, constant |

Therefore:

- ✅ **Can** detect an offset of several centimetres — the magnitude that would actually mislead
  a user.
- ❌ **Cannot** confirm or refute a millimetre-level offset. No free height point in Germany
  will, because the geoid model is the limit.

Stating this is part of the result. A number without its error budget is what got us here.

**To beat the 2 cm floor** you need a point with a published **ETRS89 ellipsoidal height** — a
Geodätischer Grundnetzpunkt (GGP) or a TP record that carries one — which removes the geoid
model from the chain entirely. Worth one email to `geodaten@lgl.bwl.de`; do not expect a
convenient one nearby.

---

## Candidate control points

All are **Höhenfestpunkte 1. Ordnung**, from the LGL AFIS open-data WFS. Heights are DHHN2016
(NHN), given to the millimetre.

**Test in the Rhine plain around Bühl, not in the Bühlertal valley.** The phase-centre offset
is a property of the antenna, not of the site — so measure it at the best site reachable. The
valley imposes a terrain horizon of 20°+ over whole sectors, which biases geometry
systematically and in the same direction every session. That is precisely the error class this
test is trying to isolate, so do not invite it.

### Shortlist

Ranked by **findability × sky view**. Every entry below is inscribed on the object itself —
that inscription is your proof you have the right stone, plate or wall, and it is the single
most useful field in the record.

**1 · `7214900023` — 135.062 m — B3, Feldkreuz** · [map](https://www.google.com/maps?q=48.707314,8.143324) · UTM32 `436974.55 / 5395274.14`
> Mauerbolzen, horizontal eingebracht
> *"Bundesstraße 3, Feldkreuz; 18,50 m von Straßenmitte, nordwestl. der Straße und
> Wirtschaftsweg, Fundamentplatte, Vorderseite, Mitte; 0,08 m unter Plattenoberkante;
> 0,1 m über Boden, Inschrift: Höhenmarke"*

Open farmland, mark only 0.10 m above ground on a concrete foundation plate, and the
description pins it to the centimetre. **The best candidate.**

**2 · `7314900078` — 137.547 m — K3764, Kreuz** · [map](https://www.google.com/maps?q=48.675380,8.121567) · UTM32 `435333.03 / 5391742.65`
> Mauerbolzen, vertikal eingebracht
> *"Kreisstraße 3764, Kreuz; 15,50 m nordwestl. der Straßenmitte, Fundamentplatte,
> Vorderseite, Mitte; 0,2 m über Boden, Inschrift: Höhenmarke"*

Wayside cross in the open, low mark, vertical bolt.

**3 · `7314900059` — 139.555 m — Granitpfeiler, Weg zum Friedenskreuz** · [map](https://www.google.com/maps?q=48.679812,8.127890) · UTM32 `435804.11 / 5392229.93`
> Pfeilerbolzen, Naturstein, **Bolzen horizontal**
> *"Granitpfeiler am südlichen Rand des Weges zum Friedenskreuz, HFP an Nordostseite des
> Pfeilers, Inschrift: Höhenmarke"*

⚠️ **The bolt is horizontal, on the north-east face of the pillar — not on top of the stone.**
This is the point where a first attempt failed to find the mark by searching the top surface.

**4 · `7314900080` — 136.946 m — K3764, Rohrdurchlaß** · [map](https://www.google.com/maps?q=48.681060,8.125389) · UTM32 `435621.57 / 5392370.72`
> Mauerbolzen, horizontal eingebracht
> *"30 m nordwestl. der Straßenmitte, an einer talseitigen Wegabzweigung; 30 m nordöstl. einer
> Straßenkreuzung, Auslauf, über Rohrmitte, in Weghöhe, Inschrift: Höhenmarke"*

~1.5 km from #3 on the same road — the natural pairing for [Test B](#test-b--height-difference-validates-the-setup).

**5 · `7314900081` — 135.689 m — K3764, Wasserschacht** · UTM32 `435671 / 5392110`
> Mauerbolzen, vertikal eingebracht
> *"10,60 m nordwestl. der Straßenmitte, im Grünstreifen ... auf dem Schacht, in der westlichen
> Ecke; 1,1 m unter Straßenhöhe, in Bodenhöhe, Inschrift: Höhenmarke"*

Open, but sits 1.1 m below road level — a slightly worse horizon.

### Fallback — town centre

Precisely described but hemmed in by buildings. Usable only because you transfer the height
rather than occupy the mark, and even then the level run gets long.

| Punkt | Height | Marker | Where |
|---|---|---|---|
| `7314900085` | 137.631 m | Turmbolzen mit Aufschrift `TP` | Rathaus, Ostseite, 0,48 m über Pflaster. Co-located trig point |
| `7314900056` | 139.251 m | Mauerbolzen, horizontal | Hauptstraße 94, Amtsgericht, 0,28 m über Gehweg |
| `7314900008` | 140.364 m | Mauerbolzen, horizontal | Lindenkirche, West-/Eingangsseite, Inschrift `1232` |
| `7314900082` | 135.709 m | Mauerbolzen, horizontal | Hauptstraße 1a, Tankstelle, 0,20 m über Pflaster |

### Reading a Lagebeschreibung

| Phrase | Meaning |
|---|---|
| `von der Straßenmitte` | Measured from the **centre line** of the road, not the kerb |
| `unter Sockeloberkante` | Below the top edge of the building plinth |
| `über Gehweg / Boden / Pflaster` | Height of the bolt above the pavement or ground |
| `Vorderseite, Mitte` | Front face of the foundation plate, centred |
| `Inschrift: …` | Text stamped on the object — your identity confirmation |
| `horizontal eingebracht` | Bolt protrudes **sideways from a vertical face** |
| `vertikal eingebracht` | Bolt points **up out of a horizontal surface** |

**Horizontal vs vertical is what makes a mark findable or not.** A horizontal bolt is invisible
from above; look at the *face* named in the description, at the stated height above ground.

The published height refers to the **crown of the bolt** — rest the staff foot on the bolt head,
not on the stone or plate around it.

> **Do not use these points for a horizontal check.** They are *Höhen*festpunkte: height only.
> Their horizontal coordinates exist so you can find the object, and are not survey-grade. The
> lat/lon above are converted from the published EPSG:25832 values for map pins only — on site,
> navigate by the description.

### Pull the data yourself

```
https://owsproxy.lgl-bw.de/owsproxy/wfs/WFS_LGL-BW_AFIS_Hoehenfestpunkte
  ?service=WFS&version=2.0.0&request=GetFeature
  &typeNames=nora:v_hoehenfestpunkt_ordnung_1
  &outputFormat=application/json
  &bbox=434000,5391000,441000,5397000,urn:ogc:def:crs:EPSG::25832
```

Feature types: `nora:v_hoehenfestpunkt_ordnung_1` (1. Ordnung), `_2` (2. Ordnung), `_3`
(Nivellementpunkte). Also loads directly into QGIS as a WFS layer, alongside the LGL orthophoto
WMS for sky-view screening.

---

## Prerequisites

**Define your Antenna Reference Point (ARP) and never change it.** Pick a physical feature of
the dome you can put a tape on — the shoulder of the 5/8″ thread, or the flat underside of the
mount — photograph it, and record the choice here. Every height in this test is referred to
that point. An undocumented ARP makes the whole exercise unrepeatable.

- Tripod or survey pole with a **fixed, known** height, ideally with a bubble level
- Levelling staff and a spirit or laser level for the height transfer
- SAPOS BW account — [Open SAPOS](https://www.sapos-bw.de/dienste.php) is free of charge
  (one-time €150 administrative fee per account); mountpoint `VRS_3_4G_BW`. As of 1 Jan 2026
  only HEPS and GPPS remain
- A phone running SW Maps with **Log To File** enabled, or the
  [dashboard](https://hmd83.github.io/ESP_RTK_ROVER/web/rtk-monitor.html) raw NMEA log

**Use SAPOS for this test, not Centipede or RTK2go.** The network's reference antennas are
themselves calibrated and its frame is unambiguous, which removes a variable rather than adding
one.

---

## Test A — absolute height (finds the offset)

You do **not** need to stand on the mark. Almost every HFP here is a wall bolt you cannot
centre over, and for a vertical test that does not matter: set up wherever the sky is best
within a few metres, then transfer the height across. The transfer costs a few millimetres
against a 2 cm floor.

1. Set the rover up within a few metres of the mark, **choosing the spot for sky view** — away
   from walls, vehicles, trees and overhead lines.
2. Transfer the mark's height to your ARP with staff and level. Record the transfer, with a
   photo. → `H_ARP` (NHN).
3. Connect SAPOS, wait for **RTK FIXED**, then let it settle at least **5 minutes** before
   recording anything.
4. Log **≥ 15 minutes** of NMEA at 1 Hz without touching the setup.
5. Record correction age, satellite count and PDOP throughout. Discard the session if the
   solution drops out of fixed.
6. **Repeat on a different day and at a different time of day.** Different satellite geometry
   and different multipath is the entire point; two sessions an hour apart prove nothing.

---

## Test B — height difference (validates the setup)

Occupy two marks and compare the measured height difference against the levelled one. Over a
short baseline the geoid error is common-mode and largely cancels, so this checks your
procedure to a much finer tolerance than Test A can.

**A constant antenna offset also cancels in a difference.** This test therefore cannot find the
offset — it exists to prove that everything *else* is sound, so that Test A's residual can be
attributed rather than guessed at.

Suggested pair: `7314900059` and `7314900080` — ~1.5 km apart, both on the same rural road.

---

## Reduction

**Use ellipsoidal height. Never use the GGA MSL field.**

GGA reports MSL altitude computed with the receiver's own internal geoid model, which is not
accurate enough and is not GCG2016. But GGA also carries the geoid separation it used, so
adding the two recovers the ellipsoidal height exactly, independent of that model:

```
h_ellipsoidal = GGA altitude (field 9) + GGA geoid separation (field 11)
```

The dashboard already does this — the **Ellipsoidal ht** cell in the GGA panel. That is the
number to record.

Then:

1. Average `h_ellipsoidal` across the session → `h_measured`
2. Convert to DHHN2016 using the **BKG GCG2016** service (LGL's own Höhentransformationsdienst
   converts only between physical frames — DHHN12/92/2016 — and will not do this step) →
   `H_measured`
3. **Offset = `H_measured` − `H_ARP`**

A positive offset means the receiver places the phase centre above the point you measure to.

---

## Sanity assertions

Check these before believing any result. Each catches a different blunder.

| Assertion | Expected | Catches |
|---|---|---|
| Quasigeoid undulation ζ = `h_ellipsoidal` − `H` | **47–50 m** in Baden-Württemberg | Frame or sign error |
| Test B residual | < 1–2 cm | Setup, ARP or transfer blunder |
| Session-to-session scatter of `h_measured` | < 2 cm | Unstable site, multipath, marginal fix |
| Correction age throughout | < 2 s | Degraded corrections invalidating the session |

If ζ lands outside 47–50 m, you have a coordinate-system error, not a discovery.

---

## Interpreting the result

| Measured offset | Conclusion |
|---|---|
| < 2 cm | **Not resolvable by this method.** Report as bounded: "offset < 2 cm, limited by GCG2016" |
| 2–5 cm | Real and characterisable. Publish it as a constant correction |
| > 5 cm | Suspect a blunder first — ARP, height transfer, frame. Re-check before publishing |

Whatever comes out gets published here, including if it is unflattering. A rover whose bias is
measured and stated is more useful than one whose bias is unknown and flattering.

---

## Results

### Session 1 — 2026-08-23 — `7314900059` — frame validated, offset not yet derivable

Raw data: [`Antenna_Layer1.geojson`](Antenna_Layer1.geojson) — two features, the published
control point and the measured RTK point.

| | |
|---|---|
| Published NHN height of mark | 139.555 m |
| Recorded ellipsoidal height | 188.257 m |
| SW Maps instrument height | applied before recording — **value unconfirmed, 1.12 m or 1.20 m** |
| → recorded value represents | the **pole tip**, already reduced; not the antenna |
| Difference vs mark | **+48.702 m** |
| ζ sanity band (47–50 m) | ✅ **pass** |
| Height of pole tip above the bolt | ❌ unknown — bolt not located on site |
| **Phase-centre offset** | **not derivable from this session** |

**What this session established.** 48.702 m falls inside the quasigeoid band, so there is no
frame or sign error in the chain. It also proves the export carries **ellipsoidal** height and
not MSL — two orthometric heights could not differ by 48.7 m. That is the failure mode that
sinks most first attempts, and it is now ruled out.

**Why the offset still cannot be extracted.** The measured difference is

```
48.702 m  =  ζ_true  +  (height of antenna ARP above the bolt)
```

— one equation, two unknowns. It needs `ζ_true` from the BKG GCG2016 service at
48.6798 N / 8.1279 E, and the tape measurement from the bolt to the ARP.

**A geometric inconsistency worth chasing.** With the instrument height applied, the recorded
value is the pole tip, so `pole tip (NHN) = 188.257 − ζ_true`. For ζ in 48.0–48.5 m that puts
the tip 0.2–0.7 m *above* the bolt — impossible if the pole stood on the ground and the bolt is
on the pillar face above it. The geometry only closes for ζ ≳ 48.7 m, the top of the plausible
band. So one of these is true, and ζ_true decides which:

- the instrument height was not in fact applied to the stored point,
- the ground at the setup was higher than the bolt (the pillar may sit downslope of the path), or
- ζ at this location really is ≈ 48.7 m.

**Open item: the instrument height itself is unconfirmed** — reported as both 1.12 m and 1.20 m.
An 8 cm ambiguity is four times the resolution this test is aiming at. It must be settled with a
tape against a photographed reference feature before any session is reducible.

**Why the mark was not found.** The bolt is **horizontal, on the north-east face** of the
granite pillar. The search was made on the top surface. See the shortlist entry above; the
`vermarkung_name` field distinguishes `horizontal` from `vertikal eingebracht` and is the field
to read before travelling.

**Horizontal residual is not reported.** The 0.17 m horizontal difference between the two
features in the file is meaningless — `7314900059` is a height-only point whose horizontal
coordinate is not survey-grade, and the antenna was deliberately not set over the mark.

**Next session:** `7214900023` (B3 Feldkreuz) — open farmland, mark 0.10 m above ground on a
foundation plate, unambiguous description. Measure bolt-to-ARP before leaving the point.

### Session log

| Date | Point | Sessions | h_measured | H_measured | H_ARP | Offset | Notes |
|---|---|---|---|---|---|---|---|
| 2026-08-23 | 7314900059 | 1 | 188.257 m (pole tip) | — | — | — | Frame validated; mark not found; IH unconfirmed |

---

## Data sources and attribution

- **Höhenfestpunkte** — Landesamt für Geoinformation und Landentwicklung Baden-Württemberg
  (LGL), AFIS open-data WFS. Licence: *Datenlizenz Deutschland – Namensnennung – Version 2.0*
  (`dl-de/by-2-0`). Attribution: **© LGL Baden-Württemberg (dl-de/by-2-0)**, see
  [lgl-bw.de/agb](https://www.lgl-bw.de/agb/)
- **GCG2016 quasigeoid** — Bundesamt für Kartographie und Geodäsie (BKG), open data
- **Corrections** — SAPOS® Baden-Württemberg (LGL)
- **Point locations** — [LGL Festpunktportal](https://festpunktportal.lgl-bw.de/festpunkte/)
