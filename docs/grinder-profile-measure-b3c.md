# B3c – Siebtraeger- und Trichtergewichte einmessen (SSR AUS)

- Die vorhandene WebUI-Kalibrierung ist weiterhin fuer die HX711-Kalibrierung mit bekanntem Gewicht zustaendig.
- WebUI Einstellungen → Waage & Gefaesse → Siebtraeger + Trichter einmessen.
- Pro Profil leere Waage tarieren, zuerst Siebtraeger allein stabil messen, dann Trichter auflegen und die Kombination stabil messen.
- Der Trichterwert wird als Differenz berechnet. Beide Werte werden erst nach gueltiger Kombination gespeichert, in den **bestehenden** NVS-Arrays `savedWeightST` und `savedWeightTri` (Namespace `savedValues`).
- Die B3a-Stabilitaetserkennung muss fuer BEIDE Erfassungen einen frischen stabilen Messwert melden. Plausibilitaet: Siebtraeger 30..1200 g, Trichter 5..300 g, Gesamtgewicht hoechstens 1500 g.
- Bei Abbruch oder fehlgeschlagener Aufnahme wird kein neues Profil gespeichert. Eine Unterbrechung waehrend zweier NVS-Schreibvorgaenge ist technisch nicht vollstaendig transaktionssicher; ggf. Profil neu einmessen.
- Der neue Assistent wartet fuer seine einzelnen Schritte auf die WebSocket-ACK-Bestaetigung (bei ERROR kein Seitenwechsel).
- Der B3b-Detektor laedt die neuen Gewichte nach Schliessen des Wizards neu und bleibt dabei weiterhin PREVIEW ONLY. Alte Legacy-Autodetect-Tara ist weiterhin deaktiviert.
- Keine Verbindung zum Grinder-Controller oder GPIO16; das SSR wird ausschliesslich auf AUS initialisiert.

## Hardwaretest
1. USB allein, Muehle und Netzversorgung abgeklemmt; im WebUI WLAN-Verbindung nutzen.
2. Bei Bedarf erst WebUI → Waage kalibrieren mit Referenzgewicht.
3. Ein Profil mit leerer Waage tarieren, ST ohne Trichter auflegen, erst stabil messen, Trichter drauf, erst stabil speichern.
4. NVS-Werte in der WebUI pruefen; nach Neustart sollten sie erhalten sein.
5. Autodetect EIN, Einmesskombination erneut auflegen. Die serielle Diagnose sollte `WaitFunnel` / `CombinationPreview` melden, aber keine automatische Tara ausloesen.
6. Unbekanntes Gefaess weiterhin `UnknownVessel`; keine Mahlfreigabe.
