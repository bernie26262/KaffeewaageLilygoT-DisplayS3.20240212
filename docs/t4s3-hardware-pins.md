# T4-S3 Hardware / Pinbelegung

Diese Datei dokumentiert die geplante Verdrahtung der Kaffeewaage mit dem
LILYGO T4-S3 AMOLED Touch. Der aktuelle T4-S3/LVGL-Prototyp läuft weiterhin im
Simulator-Modus ohne angeschlossene Wägezelle; die Pins sind aber bereits im
Code reserviert.

## Grundsatz

Die neue T4-S3-Waage soll ein reines Touch-Gerät werden:

- keine Hardware-Buttons
- kein Rotary Encoder
- keine weiteren Ein-/Ausgänge außer der Wägezelle über HX711

Dadurch bleibt die Verdrahtung sehr einfach.

## Versorgung

Gemessene Spannungen am Board bei USB-Versorgung:

| Pin | Messwert | Bedeutung |
| --- | ---: | --- |
| VBUS gegen GND | ca. 5,0 V | USB-5-V-Bus / geeigneter 5-V-Versorgungspunkt |
| BAT gegen GND | ca. 4,1 V | LiPo-/Batteriepfad, **kein 5-V-Eingang** |
| 3V3 gegen GND | ca. 3,3 V | geregelte 3,3-V-Logikversorgung |

Empfohlene Versorgung für die fertige Waage:

```text
USB-Netzteil 5 V  -> T4-S3 VBUS + GND
T4-S3 3V3         -> HX711 VCC
T4-S3 GND         -> HX711 GND
```

Der Batterieanschluss `+ / -` beziehungsweise `BAT` ist für eine einzelne
LiPo-Zelle gedacht und darf nicht mit 5 V gespeist werden.

## HX711-Anschluss

Empfohlene Pins am T4-S3-Header:

```text
T4-S3 IO42  -> HX711 DT / DOUT
T4-S3 IO41  -> HX711 SCK
T4-S3 3V3   -> HX711 VCC
T4-S3 GND   -> HX711 GND
```

Diese Pins sind im Code bereits in `src/t4s3_pins.h` vorgesehen:

```cpp
static constexpr uint8_t HX711_DOUT_PIN = 42;
static constexpr uint8_t HX711_SCK_PIN  = 41;
```

Die HX711-Versorgung über 3,3 V ist bewusst gewählt, damit die HX711-Logikpegel
zu den ESP32-S3-GPIOs passen. Der HX711 sollte nicht mit 5 V betrieben werden,
wenn seine Datenleitungen direkt an ESP32-GPIOs angeschlossen sind.

## Wägezelle

Typische 4-Draht-Wägezelle:

```text
Rot      -> E+
Schwarz  -> E-
Grün     -> A+ / S+
Weiß     -> A- / S-
```

Falls das Gewicht später negativ läuft, können `A+` und `A-` am HX711 getauscht
werden oder die Richtung wird im Code invertiert.

## Twisted Pairs

Für die Leitungen der Wägezelle sind verdrillte Paare sinnvoll:

```text
Rot + Schwarz   -> Versorgung / Erregung
Grün + Weiß     -> Messsignal / Differenzsignal
```

Die verdrillten Paare helfen, Störungen gleichmäßiger in beide Leitungen eines
Paares einzukoppeln und damit im Differenzsignal besser zu unterdrücken.

## Hinweise zur späteren Integration

Der aktuelle T4-S3-Build verwendet weiterhin:

```text
-DCOFFEE_WEIGHT_SIMULATOR=1
```

Die reale HX711-Anbindung soll erst aktiviert werden, wenn Wägezelle und Mechanik
eingebaut sind. Dann sind die nächsten Schritte:

1. HX711-Library in das T4-S3-Environment aufnehmen.
2. Initialisierung mit `HX711_DOUT_PIN` und `HX711_SCK_PIN`.
3. Rohwerte im Serial Monitor prüfen.
4. Tara und Kalibrierung an die bestehende LVGL-Oberfläche anbinden.
5. Display-Wakeup per Gewichtsänderung `±30 g` aktivieren.
