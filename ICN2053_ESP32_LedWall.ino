/* 
 * ICN2053 - ESP32 - LED Wall
 * Copyright (c) 2019 TimeWaster 
 */

// the dimensions of your "screen", means the pixel size of all your modules you want to use put together in one row
#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      64
#define DISPLAY_SCAN_LINES  32 // scan lines are usually half the display height
#define EFFECT              4  // 1: Gray gradient (static)  2: Random gray static  3: Random color static  4: Animated Rainbow  5: Animated random gray static

// leave this like it is, the pins are carefully selected to not interfere with special purpose pins
// also pins with the same function are grouped together so they can be set with one command to save cpu cycles
#define PIN_CLK 2 
#define PIN_LE  27
#define PIN_OE  5
#define PIN_A   21
#define PIN_B   22
#define PIN_C   23
#define PIN_D   25 // leave this even if you don't use pin D
#define PIN_E   26 // leave this even if you don't use pin E
#define PIN_R1  14
#define PIN_G1  15
#define PIN_B1  16
#define PIN_R2  17
#define PIN_G2  18
#define PIN_B2  19

// global variables
unsigned int  displayBufferSize      = DISPLAY_WIDTH * DISPLAY_SCAN_LINES * 16; // the buffers need to hold 16 bit of image data per pixel over 6 outputs (RGB1 + RGB2)
unsigned int  displayBufferLineSize  = DISPLAY_WIDTH * 16;
unsigned int  displayNumberChips     = DISPLAY_WIDTH / 16;
unsigned char pins[14]               = {PIN_CLK, PIN_LE, PIN_OE, PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2};
unsigned int  Translate8To16Bit[256] = {0,46,92,139,186,233,280,327,375,422,470,519,567,615,664,713,762,812,861,911,961,1011,1061,1112,1163,1214,1265,1317,1368,1420,1473,1525,1578,1631,1684,1737,1791,1844,1899,1953,2007,2062,2117,2173,2228,2284,2340,2397,2453,2510,2568,2625,2683,2741,2799,2858,2917,2976,3036,3096,3156,3216,3277,3338,3399,3461,3523,3586,3648,3711,3775,3838,3902,3967,4032,4097,4162,4228,4294,4361,4428,4495,4563,4631,4699,4768,4838,4907,4978,5048,5119,5191,5262,5335,5407,5481,5554,5628,5703,5778,5853,5929,6006,6083,6160,6238,6317,6396,6476,6556,6636,6718,6799,6882,6965,7048,7132,7217,7302,7388,7475,7562,7650,7739,7828,7918,8008,8099,8191,8284,8377,8472,8567,8662,8759,8856,8954,9053,9153,9253,9355,9457,9560,9664,9769,9875,9982,10090,10199,10309,10420,10532,10645,10760,10875,10991,11109,11228,11348,11469,11591,11715,11840,11967,12094,12223,12354,12486,12620,12755,12891,13030,13169,13311,13454,13599,13746,13895,14045,14198,14352,14509,14667,14828,14991,15157,15324,15494,15667,15842,16020,16200,16383,16569,16758,16951,17146,17345,17547,17752,17961,18174,18391,18612,18837,19067,19301,19539,19783,20032,20286,20546,20812,21083,21361,21646,21938,22237,22544,22859,23183,23516,23859,24211,24575,24950,25338,25739,26153,26583,27029,27493,27975,28478,29003,29553,30130,30736,31375,32051,32767,33530,34345,35221,36167,37195,38322,39567,40959,42537,44359,46514,49151,52551,57343,65535};
unsigned char activeScreenBuffer     = 0;
unsigned char *screenBuffer[2];

/*
 * methods that are loaded into IRAM with IRAM_ATTR to avoid being loaded on call from flash (ESP-IDF command)
 * these methods need to be defined before they are used (order of calls)
 */
void IRAM_ATTR digitalWriteFast(unsigned char pinNum, boolean value) {
    // GPIO.out_w1ts sets a pin high, GPIO.out_w1tc sets a pin low (ESP-IDF commands)
    if(value) GPIO.out_w1ts = 1 << pinNum;
    else GPIO.out_w1tc = 1 << pinNum;
}

void IRAM_ATTR digitalWriteEvenFaster(unsigned long data, unsigned long mask) {
    // GPIO.out writes multiple pins at the same time (ESP-IDF command)
    GPIO.out = (GPIO.out & ~mask) | data;
}

void IRAM_ATTR sendClock() {
    digitalWriteFast(PIN_CLK, 1);
    digitalWriteFast(PIN_CLK, 0);
}

void IRAM_ATTR sendPwmClock(unsigned char clocks) {
    while (clocks--) {
    	digitalWriteFast(PIN_OE, 1);
    	digitalWriteFast(PIN_OE, 0);
    }
}

void IRAM_ATTR sendLatch(unsigned char clocks) {
    digitalWriteFast(PIN_LE, 1);
    while(clocks--) {
        sendClock();
    }
    digitalWriteFast(PIN_LE, 0);
}

void IRAM_ATTR sendScanLine(unsigned char line) {
    unsigned long scanLine = 0x00000000;

    if(line & 0x1) scanLine += 1;
    if(line >> 1 & 0x1) scanLine += 2;
    if(line >> 2 & 0x1) scanLine += 4;
    if(line >> 3 & 0x1) scanLine += 16;
    if(line >> 4 & 0x1) scanLine += 32;

    digitalWriteEvenFaster(scanLine << 21, 0x06E00000);
}

/*
 * Arduino setup + loop
 */
void setup() {
    // create screen buffers (uses ESP-IDF malloc command to find a piece of dynamic memory to hold the buffer)
    // we use double buffering to be able to manipulate one buffer while the display refresh uses the other buffer
    screenBuffer[0] = (unsigned char*)malloc(displayBufferSize);
    screenBuffer[1] = (unsigned char*)malloc(displayBufferSize);

    // fill buffers with 0
	for (unsigned int x = 0; x < displayBufferSize; x++) {
		screenBuffer[0][x] = 0;
		screenBuffer[1][x] = 0;
	}

    // setup pins
    for (unsigned char x = 0; x < 14; x++) {
        pinMode(pins[x], OUTPUT);
        digitalWrite(pins[x], LOW);
    }

    // send screen configuration to the ICN2053 chips
    // this configuration was created by @ElectronicsInFocus https://www.youtube.com/watch?v=nhCGgTd7OHg
    // also the original C++ code this Arduino version is based on was created by him
    // it is unknown to me what this configuration does, there is no information about 
    // it whatsoever on the internet (not that i could find)
    // if someone has a document describing the configuration registers please send it to me!
    sendLatch(14); // Pre-active command
    sendLatch(12); // Enable all output channels
    sendLatch(3); // Vertical sync
    sendLatch(14); // Pre-active command
    sendConfiguration(0b0001111101110000, 4); // Write config register 1 (4 latches)
    sendLatch(14); // Pre-active command
    sendConfiguration(0xffff, 6); // Write config register 2 (6 latches)
    sendLatch(14); // Pre-active command
    sendConfiguration(0b0100000011110011, 8); // Write config. register 3 (8 latches)
    sendLatch(14); // Pre-active command
    sendConfiguration(0x0000, 10); // Write config register 4 (10 latches)
    sendLatch(14); // Pre-active command
    sendConfiguration(0x0000, 2); // Write debug register (2 latches)

    // initialize the refresh and data "aquisition" tasks to run on separate cpu cores
    // this is an ESP-IDF feature to run tasks (endless loops) on different cores
    // callback, name, stack size, null, priority 3-0 (0 lowest), null, core (0 or 1)
    xTaskCreatePinnedToCore(refreshTask, "refreshTask", 1024, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(dataTask, "dataTask", 1024, NULL, 1, NULL, 0);
}

void loop() {}

/*
 * tasks
 */
void refreshTask(void *pvParameters) {
    (void) pvParameters;

    while(1) {
        LedWallRefresh();
        vTaskDelay(1); // call ESP-IDF command vTaskDelay to avoid watchdog restarting CPU
    }
}

void dataTask(void *pvParameters) {
    (void) pvParameters;

    // start time measurement    
    unsigned long start = micros();
    unsigned long time;

    // everything from this point on is not documented, implement your own data processing or generation here.
    // use: cacheWrite(*inactiveBuffer, x, y, r, g, b); to set a pixel in the inactive buffer, then swap buffers

    #if EFFECT < 3 || EFFECT == 5
        unsigned char gray = 0;
    #endif

    #if EFFECT > 3
        unsigned int pos1 = 0;
        unsigned int pos2 = 0;
        unsigned int posXY = 0;
        unsigned int displayBufferHalfScreenSize = (DISPLAY_SCAN_LINES - 1) * displayBufferLineSize;
        unsigned int posXYOutput = 0;
        unsigned int posXYInput = 0;
        unsigned char outputmask = 0x38;
        unsigned char inputmask = 0x07;
    #endif

    #if EFFECT == 4
        float widthDivRad = (float)DISPLAY_WIDTH / TWO_PI;
        float one20DegreeRad = TWO_PI / 3;
        float count = 0;
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;
    #endif

    while(1) {
        unsigned char inactiveScreenBuffer = activeScreenBuffer + 1 & 0x1;
        unsigned char *inactiveBuffer = screenBuffer[inactiveScreenBuffer];
        unsigned char *activeBuffer = screenBuffer[activeScreenBuffer];

        for (unsigned int x = 0; x < DISPLAY_WIDTH; x++) {
            for (unsigned int y = 0; y < DISPLAY_HEIGHT; y++) {
                #if EFFECT == 1
                    if(y / 8 % 2 == 0) gray = (unsigned char)(x * 256 / DISPLAY_WIDTH);
                    else gray = (unsigned char)((DISPLAY_WIDTH - x - 1) * 256 / DISPLAY_WIDTH);

                    cacheWrite(inactiveBuffer, x, y, gray, gray, gray);
                #endif

                #if EFFECT == 2
                    gray = random(256);

                    cacheWrite(inactiveBuffer, x, y, gray, gray, gray);
                #endif

                #if EFFECT == 3
                    cacheWrite(inactiveBuffer, x, y, random(256), random(256), random(256));
                #endif

                #if EFFECT > 3
                    if(y > 0 && y < DISPLAY_SCAN_LINES) {
                        posXY = (DISPLAY_SCAN_LINES - y - 1) * displayBufferLineSize + x * 16;

                        for (unsigned int bit = 0; bit < 16; bit++) {
                            pos1 = posXY + bit;
                            pos2 = pos1 + displayBufferLineSize;

                            inactiveBuffer[pos2] = activeBuffer[pos1];
                        }
                    }
                #endif
            }

            #if EFFECT > 3
                posXYOutput = x * 16;
                posXYInput = displayBufferHalfScreenSize + posXYOutput;

                for (unsigned int bit = 0; bit < 16; bit++) {
                    inactiveBuffer[posXYOutput + bit] = (inactiveBuffer[posXYOutput + bit] & ~outputmask) | ((activeBuffer[posXYInput + bit] & inputmask) << 3);
                }
            #endif

            #if EFFECT == 4
                float sine1Hertz = (float)x / widthDivRad + count;

                red =   (unsigned char)((sin(sine1Hertz                     ) + 1) * 128);
                green = (unsigned char)((sin(sine1Hertz + one20DegreeRad    ) + 1) * 128);
                blue =  (unsigned char)((sin(sine1Hertz + one20DegreeRad * 2) + 1) * 128);

                cacheWrite(inactiveBuffer, x, 0, red, green, blue);
            #endif

            #if EFFECT == 5
                gray = random(256);

                cacheWrite(inactiveBuffer, x, 0, gray, gray, gray);
            #endif
        }
        
        #if EFFECT == 4
            count += 0.1;
            if(count > TWO_PI) count -= TWO_PI;
        #endif

        // swap buffers
        activeScreenBuffer = inactiveScreenBuffer;

        // calculate time delayed by effect generation, set delay to reach fixed 30 fps
        vTaskDelay(5 / portTICK_PERIOD_MS);  // 5ms delay to allow other background tasks to do something
        time = 28333 - (micros() - start);
        if(time < 28333) delayMicroseconds(time);
        start = micros();
    }
}

/*
 * normal methods
 */
void sendConfiguration(unsigned int data, unsigned char latches) {
	unsigned char num = displayNumberChips;
    unsigned int dataMask;
    unsigned long zero = 0x00000000;
    unsigned long rgbrgbMask = 0x000FC000; // all 6 rgb pins high

    latches = 16 - latches;

    // send config data to all chips involved (4 per 64 pixel), then latch for 1 clock
    while(num--) {
        for(unsigned char x = 0; x < 16; x++) {
            dataMask = 0x8000 >> x;

            digitalWriteEvenFaster(data & dataMask ? rgbrgbMask : zero, rgbrgbMask);
            if(num == 0 && x == latches) digitalWriteFast(PIN_LE, 1);
            sendClock();
        }

        digitalWriteFast(PIN_LE, 0);
    }
}

void cacheWrite(unsigned char *buffer, unsigned int posX, unsigned int posY, unsigned char red, unsigned char green, unsigned char blue) {
    unsigned int bufferPos = posX * 16 + posY * displayBufferLineSize;
    unsigned int inputMask = 0x8000;
    unsigned char outputmask = 0x07;
    unsigned char rgb = 0x00;
    unsigned int red16Bit = Translate8To16Bit[red];
    unsigned int green16Bit = Translate8To16Bit[green];
    unsigned int blue16Bit = Translate8To16Bit[blue];

    if(posY > DISPLAY_SCAN_LINES - 1) {
        outputmask = 0x38;
        bufferPos -= DISPLAY_SCAN_LINES * displayBufferLineSize;
    }

    // data is saved in a format where the refresh method just needs to send it out instead of formatting it first, which makes it 7 fps faster
    // the highest bit of each color is saved in a char and written into the buffer, then the second highest bit and so on
    // since the rgb values of two different lines are sent at the same time, when y is bigger than the scan line area the data is written into differnt bits of the buffer data
    for(unsigned char x = 0; x < 16; x++) {
        rgb = 0x00;
        inputMask = 0x8000 >> x;

        if(red16Bit & inputMask) rgb += 1;
        if(green16Bit & inputMask) rgb += 2;
        if(blue16Bit & inputMask) rgb += 4;

        if(posY > DISPLAY_SCAN_LINES - 1) rgb <<= 3;

        // write data without changing the surrounding bits
        buffer[bufferPos + x] = (buffer[bufferPos + x] & ~outputmask) | rgb;
    }
}

void LedWallRefresh() {
    unsigned char *activeBuffer = screenBuffer[activeScreenBuffer]; // save a reference to the active buffer to avoid sending data from a different cycle in between
    unsigned int pos;
    unsigned int bufferPos;

    sendLatch(3); // send vsync

    // since the generation of the output signal in the ICN2053 chips is directly tied to the input clock signal when receiving pixel data,
    // the order and amount of clock cycles, latches, PWM clock and so on can not be changed.
	for(unsigned int y = 0; y < DISPLAY_SCAN_LINES; y++) { // 0-N scan lines * 2 = pixel height
		for(unsigned int x = 0; x < 16; x++) { // 0-15 because 1 chip has 16 outputs
            bufferPos = (y * DISPLAY_WIDTH + x) * 16;
	        sendScanLine(y % 2 * DISPLAY_SCAN_LINES / 2 + x); // sends 0-N scan lines in every 2 (4 combined) data lines
			sendPwmClock(138); // send 138 clock cycles for pwm generation inside the ICN2053 chips - this needs to be exactly 138 pules

			for(unsigned int sect = 0; sect < displayNumberChips; sect++) { // 0-N number of chips
                pos = bufferPos + sect * 16 * 16;

                for(unsigned char bit = 0; bit < 16; bit++) { // 0-16 bits of pixel data
                    digitalWriteEvenFaster(activeBuffer[pos + bit] << 14, 0x000FC000);
                    if(sect == displayNumberChips - 1 && bit == 15) digitalWriteFast(PIN_LE, 1); // send a latch for 1 clock after every chip was written to
                    sendClock();
                }

                digitalWriteFast(PIN_LE, 0);
			}
		}
	}
}
