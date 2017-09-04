/*
* AnnaxMS2C_ESP.h - Library for Annax LED MS2 Module
*
* Ansteuerung 3*48*36 LED Matrix ANNAX LED-MS2
* --------------------------------------------
* Hardware Aufbau (je Modul):
* 12x 8bit Schieberegister
* -> 96 Spalten / 18 Zeilen Matrix
*
* Physikalische LED Zeilen 01-18 entsprechen Adresszeilen 1-18 und Registerspalten 01-48
* Physikalische LED Zeilen 19-36 entsprechen Adresszeilen 1-18 und Registerspalten 49-96
* (rechte Hälfte der logischen Matrix)
*
* Eingang 16pol IDC (Draufsicht Stiftleiste auf Platine):
*              ___
*    Vcc / 5V |· ·| Vcc / 5V
*         SCK |· ·| GND
*         RCK |· ·| GND
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
* RCK übernimmt, bei fallender Flanke, die Daten der Schieberegister an deren Ausgänge. (SPI MODE2)
*
* RS0-RS4 sind Eingänge eines Demultiplexer zur Auswahl der Zeile 1-18
*
* LED aktiviert die LEDs der gewählten Zeile solange es HIGH gesetzt ist. Etwa 1-15µs pulsen!
* LED ist mir RC-Glied vor zu langen Pulsen geschützt - sollte trotzdem vermieden werden!
*
* Zur Ansteuerung aller drei Module als ein Panel werden diese einfach kaskadiert.
* Dazu den Ausgang des ersten Moduls mit dem Eingang des zweiten Moduls verbinden.
* Die DIP-Schalter müssen alle SDA1 auswählen.
*
* Um die Ansteuerung auch über einen Microcontroller zu ermöglichen der weniger als 9 GPIOs
* zur Verfügung hat, können die Zeilen-Selektoren RS0-RS4 auch über ein weiteres kaskadiertes
* Schieberegister 74HC595 angesteuert werden.
*
* Verkabelung:
*
* ESP 8266 | WeMos D1 | ANNAX LED-MS2
* -----------------------------------
* GND        GND        GND
* ---        5V         VCC
* GPIO 14    D5         SCK
* GPIO 13    D7         SDA1
* GPIO 15    D8         RCK
* GPIO 2     D4         LED
* GPIO 0     D3         RS0
* GPIO 12    D6         RS1
* GPIO 16    D0         RS2
* GPIO 4     D2         RS3 (Pull-Up!)
* GPIO 5     D1         RS4 (Pull-Down!)
*
*
*
* Ablauf im Programmcode:
* -----------------------
* Mittels Timer-Interrupt wird alle ~100µs (Wert anpassbar) die Prozedur DrawRow() aufgerufen.
* Von dieser werden die Daten einer einzelnen logischen Zeile (entspricht physikalische Zeile N und N+18)
* via SPI (SCK/MOSI) in die Schieberegister geschrieben (Taktrate 4-8MHz).
* Anschließend erfolgt die Auswahl der Zeile via RS0-RS4 (ggf. durch schreiben
* eines weiteren Byte ins externe Schieberegister).
* RCK wird kurz LOW gepulst (<1µs) womit die Daten an den Registern anliegen.
* LED wird kurz HIGH gepulst (1-10µs) wodurch die gesetzten LEDs der Zeile aufleuchten.
* Beim nächsten Aufruf wird die nächste Zeile geschrieben, usw...
* Nach 18 Zeilen wird der Frame erneut wiederholt.
* Wichtig, wenn Daten der Zeile N geschrieben und gelatched wurden, muss Zeile N-1 selektiert werden.
*
* Features:
* ---------
* + Unterstützt zwei Buffer die entweder für DooublEbuffering eingesetzt werden können, oder
*   um einen dritten Wert je Pixel (dunkel leuchtend) zu ermöglichen
* + Wechsel zwischen Zeilen- und Spaltenweiser Anordnung des Display-Buffers
* + Mit Bitmuster invertierte (XOR) Ausgabe möglich
* + Helligkeit immer beliebig regelbar
* + Zeilenfrequenz (bei Init) anpassbar
*
*/

#ifndef AnnaxMS2C_h
#define AnnaxMS2C_h

#include <Arduino.h>
#include <SPI.h>

#define AnnaxMS2_FrameBufferSize 6*36*3 // für reales layout

uint8_t AnnaxMS2_BrightPulseDelay = 7; // Pulsbreite (in Verarbeitungsschritten, siehe unten); über 15 klingt gefährlich
uint8_t AnnaxMS2_DarkPulseDelay = 2; // Pulsbreite (in Verarbeitungsschritten, siehe unten); über 15 klingt gefährlich
uint8_t AnnaxMS2_SPIBitOrder = MSBFIRST;
uint8_t AnnaxMS2_FrameBuffer1[AnnaxMS2_FrameBufferSize];
uint8_t AnnaxMS2_FrameBuffer2[AnnaxMS2_FrameBufferSize];
uint8_t* AnnaxMS2_FrontBuffer = AnnaxMS2_FrameBuffer1;
uint8_t* AnnaxMS2_BackBuffer = AnnaxMS2_FrameBuffer2;
uint8_t AnnaxMS2_FrameBufferLayout = 0; // 0 = Bytes entlang der Zeile; 1 = Bytes spaltenweise
uint8_t AnnaxMS2_FrameBufferInvert = 0;
uint16_t AnnaxMS2_RowInterval = 100; // [µs] 800 ist flimmerfrei; für Graustufen brauchts 500;
uint8_t AnnaxMS2_InitDone = 0;
uint8_t AnnaxMS2_UseScanRowMap = 0;
volatile uint8_t AnnaxMS2_GlobalRow = 0;
volatile uint8_t AnnaxMS2_SyncFlag = 0;
volatile uint8_t AnnaxMS2_UseGreyscale = 0;
volatile uint8_t AnnaxMS2_GreyscaleIndex = 0;
volatile uint8_t AnnaxMS2_RowPulseDelay = 0;


static const uint8_t AnnaxMS2_RowMask[18] = {
	B00010000,
	B00010001,
	B00010010,
	B00010011,
	B00010100,
	B00010101,
	B00010110,
	B00010111,
	B00011000,
	B00011110,
	B00011101,
	B00011100,
	B00011011,
	B00011010,
	B00011001,
	B00011111,
	B00000000,
	B00000001,
};

static const uint16_t AnnaxMS2_RowByteSelectorH[36] = {
	341,340,339,338,337,336,
	17,16,15,14,13,12,
	335,334,333,332,331,330,
	11,10,9,8,7,6,
	329,328,327,326,325,324,
	5,4,3,2,1,0
};

static const uint16_t AnnaxMS2_RowByteSelectorV[36] = {
	18,54,90,126,162,198,
	0,36,72,108,144,180,
	234,270,306,342,378,414,
	216,252,288,324,360,396,
	450,486,522,558,594,630,
	432,468,504,540,576,612
};

static const uint16_t AnnaxMS2_ScanRowMap[18] = {
	0,17,1,16,2,15,3,14,4,13,5,12,6,11,7,10,8,9
};

void AnnaxMS2_DrawRow();
void AnnaxMS2_Init();


void AnnaxMS2_Init() {
	// https://github.com/esp8266/Arduino/blob/master/cores/esp8266/esp8266_peri.h#L189
	// Setup SPI
	SPI.begin();
	SPI.setDataMode(SPI_MODE3);
	SPI.setHwCs(false); // Disable CS on D8 / IO15 / CS / HSPI_CS used for RCK
	SPI.setFrequency(8e6);
	SPI1U1 &= ~(SPIMMOSI << SPILMOSI); // Clear MOSI length
	SPI1U1 |= ((36 * 8 - 1) << SPILMOSI); // Set MOSI length to 36 bytes

	// Setup UART1
	Serial1.begin(250000); // 500.000 = 2µs pro symbol
	U1F = 0xFF;
	U1S |= 0x08 << USTXC; // Set TX FIFO counter to 8 bits
	U1C0 |= 1 << UCTXI; // Invert TX 
	U1D = 80 * 5; // 5µs

	timer1_disable();
	timer1_attachInterrupt(AnnaxMS2_DrawRow);
	timer1_isr_init();
	timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
	timer1_write(AnnaxMS2_RowInterval*80); // 400 für 20 mal loop

	AnnaxMS2_FrontBuffer[0] = B10000001;
	AnnaxMS2_FrontBuffer[17] = B11000001;
	AnnaxMS2_FrontBuffer[630] = B10000111;
	AnnaxMS2_FrontBuffer[647] = B11110001;
	AnnaxMS2_FrontBuffer[1] = B11110000;
	AnnaxMS2_BackBuffer[0] = B10000001;
	AnnaxMS2_BackBuffer[17] = B11000001;
	AnnaxMS2_BackBuffer[630] = B10000111;
	AnnaxMS2_BackBuffer[647] = B11110001;
	AnnaxMS2_BackBuffer[1] = B00001111;

	pinMode(0, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT);
	pinMode(12, OUTPUT);
	pinMode(15, OUTPUT);
	pinMode(16, OUTPUT);

	AnnaxMS2_InitDone = 1;
}

uint8_t* AnnaxMS2_GetFrontBuffer() {
	return AnnaxMS2_FrontBuffer;
}

uint8_t* AnnaxMS2_GetBackBuffer() {
	return AnnaxMS2_BackBuffer;
}


void AnnaxMS2_SetRowInterval(uint16_t val) {
	if (val < 50) val = 50;
	AnnaxMS2_RowInterval = val;
	if (AnnaxMS2_InitDone) {
		timer1_disable();
		timer1_attachInterrupt(AnnaxMS2_DrawRow);
		timer1_isr_init();
		timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
		timer1_write(AnnaxMS2_RowInterval * 80);
	}
}

void AnnaxMS2_SwapBuffer() {
	uint8_t* tmp;
	tmp = AnnaxMS2_FrontBuffer;
	while (!AnnaxMS2_SyncFlag); // wait for sync!
	AnnaxMS2_FrontBuffer = AnnaxMS2_BackBuffer;
	AnnaxMS2_BackBuffer = tmp;
}

void AnnaxMS2_SwapBufferCopy() {
	uint8_t* tmp;
	tmp = AnnaxMS2_FrontBuffer;
	while (!AnnaxMS2_SyncFlag); // wait for sync!
	AnnaxMS2_FrontBuffer = AnnaxMS2_BackBuffer;
	AnnaxMS2_BackBuffer = tmp;
	for (uint16_t i = 0; i < AnnaxMS2_FrameBufferSize; i++) AnnaxMS2_BackBuffer[i] = AnnaxMS2_FrontBuffer[i];
}


void AnnaxMS2_SetFrontBuffer(uint8_t* pBuffer) {
	AnnaxMS2_FrontBuffer = pBuffer;
}

void AnnaxMS2_SetBitOrder(uint8_t bitOrder) {
	AnnaxMS2_SPIBitOrder = bitOrder;
	if (AnnaxMS2_InitDone) SPI.setBitOrder(bitOrder);
}

void AnnaxMS2_SetUseScanRowMap(uint8_t val) {
	AnnaxMS2_UseScanRowMap = val;
}

void AnnaxMS2_SetFrameBufferLayout(uint8_t layout) {
	AnnaxMS2_FrameBufferLayout = layout;
}

void AnnaxMS2_SetFrameBufferInvert(uint8_t inv) {
	AnnaxMS2_FrameBufferInvert = inv;
}

void AnnaxMS2_SetBrightPulseDelay(uint8_t val) {
	if (val > 15) val = 15;
	AnnaxMS2_BrightPulseDelay = val;
}

void AnnaxMS2_SetDarkPulseDelay(uint8_t val) {
	if (val > 15) val = 15;
	AnnaxMS2_DarkPulseDelay = val;
}

void AnnaxMS2_SetUseGreyscale(uint8_t val) {
	AnnaxMS2_UseGreyscale = (val & 0x01);
}

uint8_t AnnaxMS2_GetSyncFlag() {
	return AnnaxMS2_SyncFlag;
}

void AnnaxMS2_WaitSync() {
	while (!AnnaxMS2_GetSyncFlag());
}


volatile uint8_t row = 0; // Physische Zeile
ICACHE_RAM_ATTR void AnnaxMS2_DrawRow() {
	uint8_t fpos = 0;
	uint8_t tByte = 0;
	uint8_t* displayBuffer = AnnaxMS2_FrontBuffer;
	uint32_t rowBuffer32[10];
	uint8_t *rowBuffer = (uint8_t *)&rowBuffer32; // byte-weiser Zugriff auf 

	// RCK pulsen und Zeilenwahl festlegen. Damit liegen Daten des vorigen Laufs an!
	GPOC = 0x9031; // 1001 0000 0011 0001 // Clear GPIO 0,4,5,12,15 LOW
	GP16O &= ~1; // Clear GPIO16
	GPOS = (1 & (AnnaxMS2_RowMask[row] >> 0)) << 0; // GPIO 0 / RS0
	GPOS = (1 & (AnnaxMS2_RowMask[row] >> 1)) << 12; // GPIO 12 / RS1
	GP16O |= 1 & (AnnaxMS2_RowMask[row] >> 2); // GPIO 16 / RS2
	GPOS = (1 & (AnnaxMS2_RowMask[row] >> 3)) << 4; // GPIO 4 / RS3
	GPOS = (1 & (AnnaxMS2_RowMask[row] >> 4)) << 5; // GPIO 5 / RS4
	GPOS = (1 << 15) ; // GPIO GPIO15 HIGH (RCK Pulse)

	// LED Pulsdauer einstellen
	U1D = 80 * AnnaxMS2_BrightPulseDelay;
	if (AnnaxMS2_UseGreyscale && AnnaxMS2_GreyscaleIndex) U1D = 80 * AnnaxMS2_DarkPulseDelay;
	// Alles vorbereitet, LED Puls folgt ganz am Ende um Daten/Zeilensignalen noch etwas Zeit zu geben
	
	
	// *** Nächste Zeile ermitteln ***
	// Interlace rows;
	AnnaxMS2_GlobalRow++;
	if (AnnaxMS2_GlobalRow >= 18) AnnaxMS2_GlobalRow = 0;
	if (AnnaxMS2_UseScanRowMap) {
		row = AnnaxMS2_ScanRowMap[AnnaxMS2_GlobalRow];
	}
	else {
		row = AnnaxMS2_GlobalRow;
	}

	if (AnnaxMS2_UseGreyscale) {
		if (AnnaxMS2_GlobalRow == 0) AnnaxMS2_GreyscaleIndex ^= 0x01;
		if (AnnaxMS2_GreyscaleIndex) displayBuffer = AnnaxMS2_BackBuffer;
	}

	volatile uint32_t * fifoPtr = &SPI1W0; // SPI1W0 - SPI1W15 32bit FIFO Buffer registers
	uint16_t rowOffset = row * 18;
	AnnaxMS2_SyncFlag = 0;

	if (AnnaxMS2_FrameBufferLayout == 0) {
		// Bytes Zeilenweise
		rowBuffer[0] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[0] + rowOffset];
		rowBuffer[1] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[1] + rowOffset];
		rowBuffer[2] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[2] + rowOffset];
		rowBuffer[3] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[3] + rowOffset];
		rowBuffer[4] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[4] + rowOffset];
		rowBuffer[5] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[5] + rowOffset];
		rowBuffer[6] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[6] + rowOffset];
		rowBuffer[7] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[7] + rowOffset];
		rowBuffer[8] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[8] + rowOffset];
		rowBuffer[9] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[9] + rowOffset];
		rowBuffer[10] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[10] + rowOffset];
		rowBuffer[11] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[11] + rowOffset];
		rowBuffer[12] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[12] + rowOffset];
		rowBuffer[13] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[13] + rowOffset];
		rowBuffer[14] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[14] + rowOffset];
		rowBuffer[15] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[15] + rowOffset];
		rowBuffer[16] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[16] + rowOffset];
		rowBuffer[17] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[17] + rowOffset];
		rowBuffer[18] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[18] + rowOffset];
		rowBuffer[19] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[19] + rowOffset];
		rowBuffer[20] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[20] + rowOffset];
		rowBuffer[21] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[21] + rowOffset];
		rowBuffer[22] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[22] + rowOffset];
		rowBuffer[23] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[23] + rowOffset];
		rowBuffer[24] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[24] + rowOffset];
		rowBuffer[25] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[25] + rowOffset];
		rowBuffer[26] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[26] + rowOffset];
		rowBuffer[27] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[27] + rowOffset];
		rowBuffer[28] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[28] + rowOffset];
		rowBuffer[29] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[29] + rowOffset];
		rowBuffer[30] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[30] + rowOffset];
		rowBuffer[31] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[31] + rowOffset];
		rowBuffer[32] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[32] + rowOffset];
		rowBuffer[33] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[33] + rowOffset];
		rowBuffer[34] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[34] + rowOffset];
		rowBuffer[35] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorH[35] + rowOffset];
	}
	else {
		// Bytes Spaltenweise
		rowBuffer[0] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[0] + row];
		rowBuffer[1] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[1] + row];
		rowBuffer[2] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[2] + row];
		rowBuffer[3] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[3] + row];
		rowBuffer[4] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[4] + row];
		rowBuffer[5] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[5] + row];
		rowBuffer[6] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[6] + row];
		rowBuffer[7] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[7] + row];
		rowBuffer[8] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[8] + row];
		rowBuffer[9] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[9] + row];
		rowBuffer[10] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[10] + row];
		rowBuffer[11] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[11] + row];
		rowBuffer[12] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[12] + row];
		rowBuffer[13] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[13] + row];
		rowBuffer[14] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[14] + row];
		rowBuffer[15] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[15] + row];
		rowBuffer[16] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[16] + row];
		rowBuffer[17] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[17] + row];
		rowBuffer[18] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[18] + row];
		rowBuffer[19] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[19] + row];
		rowBuffer[20] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[20] + row];
		rowBuffer[21] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[21] + row];
		rowBuffer[22] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[22] + row];
		rowBuffer[23] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[23] + row];
		rowBuffer[24] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[24] + row];
		rowBuffer[25] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[25] + row];
		rowBuffer[26] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[26] + row];
		rowBuffer[27] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[27] + row];
		rowBuffer[28] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[28] + row];
		rowBuffer[29] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[29] + row];
		rowBuffer[30] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[30] + row];
		rowBuffer[31] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[31] + row];
		rowBuffer[32] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[32] + row];
		rowBuffer[33] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[33] + row];
		rowBuffer[34] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[34] + row];
		rowBuffer[35] = AnnaxMS2_FrameBufferInvert ^ displayBuffer[AnnaxMS2_RowByteSelectorV[35] + row];
	}

	if (AnnaxMS2_GlobalRow == 17) AnnaxMS2_SyncFlag = 1; // Sync an, da letzte Zeile in RAM gelesen wurde

	fifoPtr[0] = rowBuffer32[0];
	fifoPtr[1] = rowBuffer32[1];
	fifoPtr[2] = rowBuffer32[2];
	fifoPtr[3] = rowBuffer32[3];
	fifoPtr[4] = rowBuffer32[4];
	fifoPtr[5] = rowBuffer32[5];
	fifoPtr[6] = rowBuffer32[6];
	fifoPtr[7] = rowBuffer32[7];
	fifoPtr[8] = rowBuffer32[8];

	SPI1CMD |= SPIBUSY; // Start sending via SPI from FIFO
	U1F = 0xFF; // LED Pulse
}








#endif
