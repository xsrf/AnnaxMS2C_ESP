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
 
#include <AnnaxMS2C_ESP.h>


uint8_t* frontBuffer;
uint8_t* backBuffer;

void setup() { 
  AnnaxMS2_Init();
  AnnaxMS2_SetBitOrder(MSBFIRST);
  frontBuffer = AnnaxMS2_GetFrontBuffer();
  backBuffer = AnnaxMS2_GetBackBuffer();
  golinit();
}


void loop() {
  gol();
}

void golinit() {
  randomSeed(analogRead(0)+analogRead(1)+analogRead(2));
  for(uint16_t i = 0; i<18*36; i++) backBuffer[i] = random(255);
}

void gol() {
  // Feld ist 18 Byte bzw 144 Bit breit und 36 hoch;
  AnnaxMS2_SetBitOrder(MSBFIRST);
  AnnaxMS2_SwapBufferCopy();
  uint8_t* m = AnnaxMS2_GetBackBuffer();
  uint8_t prevLine[18]; // sichert den vorherigen Zustand der Zeile drüber
  uint8_t thisLine[18]; // sichert den vorherigen Zustand der aktuellen Zeile
  uint8_t alives = 0;
  for(uint8_t i = 0; i<18; i++) prevLine[i] = 0;
  for(uint8_t y = 0; y<36; y++) {
    for(uint8_t i = 0; i<18; i++) thisLine[i] = m[y*18 + i];
    for(uint8_t x = 0; x < 144; x++) {
      alives = 0; // lebende Nachbarn
      if(x>0 && y>0 && ((prevLine[(x-1)>>3]>>((x-1)&7))&1)>0 ) alives++; // lebt links drüber?
      if(y>0 && ((prevLine[(x)>>3]>>((x)&7))&1)>0 ) alives++; // lebt drüber?
      if(x<143 && y>0 && ((prevLine[(x+1)>>3]>>((x+1)&7))&1)>0 ) alives++; // lebt rechts drüber?
      if(x>0 && ((thisLine[(x-1)>>3]>>((x-1)&7))&1)>0 ) alives++; // lebt links?
      if(x<143 && ((thisLine[(x+1)>>3]>>((x+1)&7))&1)>0 ) alives++; // lebt rechts?
      if(x>0 && y<35 && ((m[(y+1)*18 + ((x-1)>>3)]>>((x-1)&7))&1)>0 ) alives++; // lebt links drunter?
      if(y<35 && ((m[(y+1)*18 + ((x)>>3)]>>((x)&7))&1)>0 ) alives++; // lebt drunter?
      if(x<143 && y<35 && ((m[(y+1)*18 + ((x+1)>>3)]>>((x+1)&7))&1)>0 ) alives++; // lebt rechts drunter?

      if(alives < 2 || alives > 3) {
        // Zelle stirbt auf jeden Fall -> 0
        m[y*18 + (x>>3)] &= ~(1<<(x&7));
      }
      if(alives == 3) {
        // Zelle wird geboren oder bleibt am leben -> 1
        m[y*18 + (x>>3)] |= (1<<(x&7));
      }        
    }
    for(uint8_t i = 0; i<18; i++) prevLine[i] = thisLine[i];
  }
}

