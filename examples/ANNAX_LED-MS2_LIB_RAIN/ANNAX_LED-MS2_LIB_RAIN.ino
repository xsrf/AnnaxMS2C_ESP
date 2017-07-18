/*
 * Ansteuerung 3*48*36 LED Matrix ANNAX LED-MS2
 * --------------------------------------------
 * Aufbau (je Modul): 
 * 12x 8bit Schieberegister 
 * -> 96 Spalten / 18 Zeilen Matrix
 * 
 * Physikalische Zeilen 01-18 entsprechen Adresszeilen 1-18 und Registerspalten 01-48
 * Physikalische Zeilen 19-36 entsprechen Adresszeilen 1-18 und Registerspalten 49-96
 * (rechte Hälfte der logischen Matrix)
 * 
 * Eingang 16pol IDC (Draufsicht Stiftleiste auf Platine):
 *              ___ 
 *    Vcc / 5V |· ·| Vcc / 5V
 *         SCK |· ·| GND
 *         RCK |· ·| CS3 / GND
 *         LED |· ·  RS4 
 *         RS0 |· ·  RS1
 *         RS2 |· ·| RS3
 *        SDA4 |· ·| SDA2
 *        SDA3 |· ·| SDA1
 *              ¯¯¯
 *             
 * SCK ist der Takt für die 12 Schieberegister, eine steigende Flanke übernimmt die
 * Daten aus SDA1-SDA4 (nur ein Datenkanal wird genutzt, Auswahl erfolgt über DIP-Schalter)
 * ins Speicherregister. 
 * 
 * RCK übernimmt, bei steigender Flanke, die Daten der Schieberegister an deren Ausgänge.
 * 
 * RS0-RS4 sind Eingänge eines Demultiplexer zur Auswahl der Zeile 1-18
 * 
 * LED aktiviert die LEDs solange es HIGH gesetzt ist. Etwa 1-10µs pulsen!
 * 
 * CS3/GND lag permanent auf GND, geht an CS3 eines Demultiplexers... unbekannte Funktion.
 *
 * Zur Ansteuerung aller drei Module als ein Panel werden diese einfach kaskadiert.
 * Dazu den Ausgang des ersten Moduls mit dem Eingang des zweiten Moduls verbinden.
 * Die DIP-Schalter müssen alle SDA1 auswählen. 
 *
*/
 
#include <AnnaxMS2C.h>
uint8_t* frontBuffer;

void setup() { 
  AnnaxMS2_Init();
  AnnaxMS2_SetBitOrder(MSBFIRST);
  frontBuffer = AnnaxMS2_GetFrontBuffer();
  randomSeed(analogRead(0)+analogRead(1)+analogRead(2));
}


void loop() {
  // Bildinhalt eine Zeile nach unten verschieben - entspricht 18 Bytes
  for(uint16_t i = 18*36-1; i>=18; i--) frontBuffer[i] = frontBuffer[i-18];
  // erste Zeile mit Zufallswerten überschreiben
  for(uint8_t i = 0; i<18; i++) frontBuffer[i] = random(255)&random(255)&random(255);
  delay(20);
}


