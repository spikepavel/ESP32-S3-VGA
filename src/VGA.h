#ifndef VGA_H
#define VGA_H

#include "PinConfig.h"
#include "Mode.h"
#include "DMAVideoBuffer.h"
#include "Font.h"

class VGA
{
	
	public:
	Mode mode;
	int bufferCount;
	int bits;
	int SCREEN;
	PinConfig pins;
	int backBuffer;
	DMAVideoBuffer *dmaBuffer;
	int dmaChannel;
	
	
	public:
////////////
	int cursorX, cursorY, cursorBaseX;
	long frontColor, backColor;
	Font *font;

	bool autoScroll = true;

	int xres;
	int yres;
////////////
	
	
	
	public:
	VGA();
	~VGA();
	bool init(const PinConfig pins, const Mode mode, int bits, int SCREEN);
	bool start();
	bool show();
	void clear(int rgb = 0);
	void dot(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	void dot(int x, int y, int rgb);
	void dotFast(int x, int y, int rgb);
	void dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	void dotFast16(int x, int y, int rgb);
	void dotFast8(int x, int y, int rgb);
	int rgb(uint8_t r, uint8_t g, uint8_t b);
	
	void setTextColor(uint16_t front, uint16_t back);
	void setFont(Font &font);
	void setCursor(int x, int y);
	void drawChar(int x, int y, int ch);
	void drawCharScale(int x, int y, int ch, int scaleFont);
	
	void print(const char ch);
	void println(const char ch);
	void print(const char *str);
	void println(const char *str);
	void print(long number, int base, int minCharacters);
	void print(unsigned long number, int base, int minCharacters);
	void println(long number, int base, int minCharacters);
	void println(unsigned long number, int base, int minCharacters);
	void print(int number, int base, int minCharacters);
	void println(int number, int base, int minCharacters);
	void print(unsigned int number, int base, int minCharacters);
	void println(unsigned int number, int base, int minCharacters);
	void print(short number, int base, int minCharacters);
	void println(short number, int base, int minCharacters);
	void print(unsigned short number, int base, int minCharacters);
	void println(unsigned short number, int base, int minCharacters);
	void print(unsigned char number, int base, int minCharacters);
	void println(unsigned char number, int base, int minCharacters);
	void println();
	void print(double number, int fractionalDigits, int minCharacters);
	void println(double number, int fractionalDigits, int minCharacters);
	
	void xLineFast(int x0, int x1, int y, int rgb);
	void line(int x1, int y1, int x2, int y2, int rgb);
	void tri(int x1, int y1, int x2, int y2, int x3, int y3, int rgb);
	void fillTri(int x1, int y1, int x2, int y2, int x3, int y3, int rgb);
	void squircle(int center_x, int center_y, int a, int b, float n, int rgb);
	void fillSquircle(int center_x, int center_y, int a, int b, float n, int rgb);
	void fillRect(int x, int y, int w, int h, int rgb);
	void rect(int x, int y, int w, int h, int rgb);
	void circle(int x, int y, int r, int rgb);
	void fillCircle(int x, int y, int r, int rgb);
	void ellipse(int x, int y, int rx, int ry, int rgb);
	void fillEllipse(int x, int y, int rx, int ry, int rgb);
	void mouse(int x1, int y1);
	
	void scrollText(int dy);
	
	void get(int x, int y);
	
	void dotAdd(int x, int y, int rgb);
	
	void scrollWindow(int x, int y, int w, int h, int dx, int dy);
	
	void scrollFastWindow(int dx, int dy);
	
	protected:
	void attachPinToSignal(int pin, int signal);
};

#endif