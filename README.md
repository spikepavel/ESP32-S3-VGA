# spikepavel's ESP32-S3 VGA library
High-performance VGA graphics library for ESP32-S3.\
\
Check out https://youtube.com/@BASICOS-COMPUTER and https://basicos.ru for project updates.\
\
If you found it useful please consider supporting my work on https://donationalerts.com/r/basicos
<br />
## Installation
This library only supports the ESP32-S3.\
\
To use in the Arduino IDE, you must have ESP32-S3 support pre-installed.\
\
The ESP32-S3 library can be found in the Library Manager(Sketch -> Include Library -> Manage Libaries).\
<br />
```
#include <VGA.h>
```
## Pin configuration
An VGA cable can be used to connect to the ESP32-S3.\
\
The connector pins can be found here: https://en.wikipedia.org/wiki/VGA_connector
<br />
<br />
You can reassign any pins yourself, but keep in mind that some ESP32-S3 pins may already be occupied by something else.\
<br />
**8 bit color setup**
```
const PinConfig pins(-1,-1,6,7,8,  -1,-1,-1,12,13,14,  -1,-1,-1,18,21,  1,2); // R G B h v
```
**16 bit color setup**
```
const PinConfig pins(4,5,6,7,8,  9,10,11,12,13,14,  15,16,17,18,21,  1,2); // R G B h v
```
## Usage
```
void setup() {
	const PinConfig pins(4,5,6,7,8,  9,10,11,12,13,14,  15,16,17,18,21,  1,2);

	Mode mode = Mode::MODE_400x300x60; // 4:3 ratio

	//vga.setFrameBufferCount(2); // Double buffering is enabled using

	//if(!vga.init(pins, mode, 16, 1)) while(1) delay(1);
	if(!vga.init(pins, mode, 16, 2)) while(1) delay(1);
	//if(!vga.init(pins, mode, 8, 3))  while(1) delay(1);

	vga.start();
}
```
## Gratitude
NKros\
stimmer\
FabGL\
bitluni\
starling13