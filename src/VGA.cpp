//esp IDE 2.0.13 нужно
//esp IDE 2.0.13 need

#include "VGA.h"
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <hal/gpio_hal.h>
#include <driver/periph_ctrl.h>
#include <driver/gpio.h>
#include <soc/lcd_cam_struct.h>
#include <math.h>
#include <esp_private/gdma.h>

#ifndef min
#define min(a,b)((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b)((a)>(b)?(a):(b))
#endif

#define HAL_FORCE_MODIFY_U32_REG_FIELD(base_reg, reg_field, field_val)    \
{                                                           \
	uint32_t temp_val = base_reg.val;                       \
	typeof(base_reg) temp_reg;                              \
	temp_reg.val = temp_val;                                \
	temp_reg.reg_field = (field_val);                       \
	(base_reg).val = temp_reg.val;                          \	
}


VGA::VGA()
{
	bufferCount = 1;
	dmaBuffer = 0;
	dmaChannel = 0;
}

VGA::~VGA()
{
	bits = 0;
	SCREEN = 0;
	backBuffer = 0;
}

void VGA::attachPinToSignal(int pin, int signal)
{
	esp_rom_gpio_connect_out_signal(pin, signal, false, false);
	gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
	gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);
}

bool VGA::init(const PinConfig pins, const Mode mode, int bits, int SCREEN)
{
	this->pins = pins;
	this->mode = mode;
	this->bits = bits;
	this->SCREEN = SCREEN;
	backBuffer = 0;

	dmaBuffer = new DMAVideoBuffer(mode.vRes, mode.hRes * (bits / 8), mode.vClones, true, bufferCount);
	if(!dmaBuffer->isValid())
	{
		delete dmaBuffer;
		return false;
	}

	periph_module_enable(PERIPH_LCD_CAM_MODULE);
	periph_module_reset(PERIPH_LCD_CAM_MODULE);
	LCD_CAM.lcd_user.lcd_reset = 1;
	esp_rom_delay_us(100);

	int N = round(240000000.0/(double)mode.frequency);
	if(N < 2) N = 2;

	LCD_CAM.lcd_clock.clk_en = 1;
	LCD_CAM.lcd_clock.lcd_clk_sel = 2;

	LCD_CAM.lcd_clock.lcd_clkm_div_a = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_num = N;
	LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;		
	LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
	LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

	LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 1;
	LCD_CAM.lcd_user.lcd_2byte_en = (bits==8)?0:1;
    LCD_CAM.lcd_user.lcd_cmd = 0;
    LCD_CAM.lcd_user.lcd_dummy = 0;
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_cmd_2_cycle_en = 0;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;
    LCD_CAM.lcd_user.lcd_dout_cyclelen = 0;
	LCD_CAM.lcd_user.lcd_always_out_en = 1;
    LCD_CAM.lcd_ctrl2.lcd_hsync_idle_pol = mode.hPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_vsync_idle_pol = mode.vPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_de_idle_pol = 1;	

	LCD_CAM.lcd_misc.lcd_bk_en = 1;	
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;

	LCD_CAM.lcd_ctrl2.lcd_hsync_width = mode.hSync - 1;
    LCD_CAM.lcd_ctrl.lcd_hb_front = mode.blankHorizontal() - 1;
    LCD_CAM.lcd_ctrl1.lcd_ha_width = mode.hRes - 1;
//	if(SCREEN = 3)
//	{
//    LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.totalHorizontal() + 4;
//	}
//    else
//	{
	LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.totalHorizontal() + 1;
//	}
	LCD_CAM.lcd_ctrl2.lcd_vsync_width = mode.vSync - 1;
    HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, mode.vSync + mode.vBack - 1);
    LCD_CAM.lcd_ctrl.lcd_va_height = mode.vRes * mode.vClones - 1;
    LCD_CAM.lcd_ctrl.lcd_vt_height = mode.totalVertical();

	LCD_CAM.lcd_ctrl2.lcd_hs_blank_en = 1;
	HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, 0);

	LCD_CAM.lcd_misc.lcd_next_frame_en = 1;

	if(bits == 8)
	{
		int pins[8] = {
			this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	else if(bits == 16)
	{
		int pins[16] = {
			this->pins.r[0], this->pins.r[1], this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[0], this->pins.g[1], this->pins.g[2], this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[0], this->pins.b[1], this->pins.b[2], this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	attachPinToSignal(this->pins.hSync, LCD_H_SYNC_IDX);
	attachPinToSignal(this->pins.vSync, LCD_V_SYNC_IDX);
  
	gdma_channel_alloc_config_t dma_chan_config = 
	{
		.direction = GDMA_CHANNEL_DIRECTION_TX,
	};
	gdma_channel_handle_t dmaCh;
	gdma_new_channel(&dma_chan_config, &dmaCh);
	dmaChannel = (int)dmaCh;
	gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
	gdma_transfer_ability_t ability = 
	{
        .sram_trans_align = 4,
    };
    gdma_set_transfer_ability(dmaCh, &ability);

	xres = mode.hRes;
	yres = mode.vRes;

	return true;
}

bool VGA::start()
{
	gdma_reset((gdma_channel_handle_t)dmaChannel);
    esp_rom_delay_us(1);	
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
	esp_rom_delay_us(1);
	LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
	gdma_start((gdma_channel_handle_t)dmaChannel, (intptr_t)dmaBuffer->getDescriptor());
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_update = 1;
	LCD_CAM.lcd_user.lcd_start = 1;
	return true;
}

bool VGA::show()
{
	if(bufferCount <= 1) 
		return true;
	dmaBuffer->attachBuffer(backBuffer);
	backBuffer = (backBuffer + 1) % bufferCount;
	return true;
}

void VGA::dot(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{

	if(x >= mode.hRes || y >= mode.vRes) return;
	if(bits == 8)
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	else if(bits == 16)
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);

}

void VGA::dot(int x, int y, int rgb)
{

	if(x >= mode.hRes || y >= mode.vRes) return;
	if(bits == 8)
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb;
	else if(bits == 16)
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = rgb;

}

void VGA::dotFast(int x, int y, int rgb)
{
	if(bits == 8)
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb;
	else if(bits == 16)
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = rgb;
}

void VGA::dotFast16(int x, int y, int rgb)
{
	dmaBuffer->getLineAddr16(y, backBuffer)[x] = rgb;
}

void VGA::dotFast8(int x, int y, int rgb)
{
	dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb;
}

void VGA::dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	if(x >= mode.hRes || y >= mode.vRes) return;
	if(bits == 8)
	{
		r = min((rand() & 31) | (r & 0xe0), 255);
		g = min((rand() & 31) | (g & 0xe0), 255);
		b = min((rand() & 63) | (b & 0xc0), 255);
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	}
	else
	if(bits == 16)
	{
		r = min((rand() & 7) | (r & 0xf8), 255);
		g = min((rand() & 3) | (g & 0xfc), 255); 
		b = min((rand() & 7) | (b & 0xf8), 255);
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
	}	
}

int VGA::rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if(bits == 8)
		return (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	else if(bits == 16)
		return (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);

}

void VGA::clear(int rgb) // под каждое разрешение экрана надо вписать руками размерность и backBuffer
{                        // for each screen resolution, you need to enter the dimension and backBuffer with your hands
	if(SCREEN == 1)
	{
		for(int y = 0; y < 297; y++)
			for(int x = 0; x < 528; x++)
				dmaBuffer->getLineAddr16(y, 0)[x] = rgb;
	}
	else if(SCREEN == 2)
	{
		for(int y = 0; y < 300; y++)
			for(int x = 0; x < 400; x++)
				dmaBuffer->getLineAddr16(y, 0)[x] = rgb;
	}
	else if(SCREEN == 3)
	{
		for(int y = 0; y < 480; y++)
			for(int x = 0; x < 640; x++)
				dmaBuffer->getLineAddr8(y, 0)[x] = rgb;
	}
	
	else if(SCREEN == 4)
	{
		for(int y = 0; y < 240; y++)
			for(int x = 0; x < 320; x++)
				dmaBuffer->getLineAddr16(y, backBuffer)[x] = rgb;
	}
}

void VGA::setTextColor(uint16_t front, uint16_t back = 0)
{
	if(bits == 16) // базовый цвет
		{
		frontColor = front;
		backColor = back;
		}
	else if(bits == 8) // в режиме 640х480х8 текст цвета костыль
		{
		//front
		if(front == 0 ){ front = 0;}
		if(front == 65535 ){ front = 255;}
		if(front == 33808 ){ front = 164;}
		if(front == 52825 ){ front = 246;}
		
		if(front == 31 ){ front = 7;}
		if(front == 2016 ){ front = 56;}
		if(front == 63488 ){ front = 192;}
		
		if(front == 65504 ){ front = 248;}
		if(front == 63519 ){ front = 199;}
		if(front == 2047 ){ front = 63;}

		if(front == 1024 ){ front = 32;}
		if(front == 32768 ){ front = 128;}
		if(front == 16 ){ front = 4;}		
			
		if(front == 1040 ){ front = 36;}
		if(front == 33792 ){ front = 160;}
		if(front == 32784 ){ front = 132;}

		//back - можно убрать если не равен нулю
		if(back == 0 ){ back = 0;}
		if(back == 65535 ){ back = 255;}
		if(back == 33808 ){ back = 164;}
		if(back == 52825 ){ back = 246;}
		
		if(back == 31 ){ back = 7;}
		if(back == 2016 ){ back = 56;}
		if(back == 63488 ){ back = 192;}
		
		if(back == 65504 ){ back = 248;}
		if(back == 63519 ){ back = 199;}
		if(back == 2047 ){ back = 63;}

		if(back == 1024 ){ back = 32;}
		if(back == 32768 ){ back = 128;}
		if(back == 16 ){ back = 4;}		
			
		if(back == 1040 ){ back = 36;}
		if(back == 33792 ){ back = 160;}
		if(back == 32784 ){ back = 132;}		
				
			frontColor = front;
			backColor = back;
	}
}

void VGA::setFont(Font &font)
{
	this->font = &font;
}

void VGA::setCursor(int x, int y)
{
	cursorX = cursorBaseX = x;
	cursorY = y;
}

void VGA::drawChar(int x, int y, int ch)
{
	if (!font)
		return;
	if (!font->valid(ch))
		return;
	const unsigned char *pix = &font->pixels[font->charWidth * font->charHeight * (ch - font->firstChar)];
	for (int py = 0; py < font->charHeight; py++)
		for (int px = 0; px < font->charWidth; px++)
			if (*(pix++))
				dot(px + x, py + y, frontColor);
				else
				dot(px + x, py + y, backColor);
}
	
void VGA::drawCharScale(int x, int y, int ch, int scaleFont)  // позволяет выводить по заданным координатам символы от 0 до 255 с масштабированием. Сделал по просьбе ребят с Волгограда. У них на Векторе-06Ц такая функция встроена аппаратно в железо. Для совместимости программ.
{
		
	if (!font)
		return;
	if (!font->valid(ch))
		return;
	const unsigned char *pix = &font->pixels[font->charWidth * font->charHeight * (ch - font->firstChar)];

	for(int yy = 0; yy < font->charHeight; yy++) {
	for(int xx = 0; xx < font->charWidth; xx++) {
		int	xScale = xx*scaleFont;
		int	yScale = yy*scaleFont;
    for(int ycount = 0; ycount < scaleFont; ycount++) {
    for(int xcount = 0; xcount < scaleFont; xcount++) {  
		if (*(pix))
		{
		dot(x + xScale + xcount, y + yScale + ycount, frontColor);			
		}
		else
		{
		dot(x + xScale + xcount, y + yScale + ycount, backColor);
		} 
		}
		}	
		pix++;
		}
		}				
}

void VGA::print(const char ch)
{
	if (!font)
		return;
	if (font->valid(ch))
	{
		drawChar(cursorX, cursorY, ch);
	}
	else
	{
		drawChar(cursorX, cursorY, ' ');
	}			
		cursorX += font->charWidth;
		
	if (cursorX + font->charWidth > xres)
	{
		cursorX = cursorBaseX;
		cursorY += font->charHeight;
		if(autoScroll && cursorY + font->charHeight > yres)
		{				
			scrollText(cursorY + font->charHeight - yres);
		}	
	}
}

void VGA::println(const char ch)
{
	print(ch);
	print("\n");
}

void VGA::print(const char *str)
{
	if (!font)
		return;
	while (*str)
	{
		if(*str == '\n')
		{
			cursorX = cursorBaseX;
			cursorY += font->charHeight;
		}
		else
			print(*str);
			str++;
	}
}

void VGA::println(const char *str)
{
	print(str); 
	print("\n");
}

void VGA::print(long number, int base = 10, int minCharacters = 1)
{
	if(minCharacters < 1)
		minCharacters = 1;
	bool sign = number < 0;
	if (sign)
		number = -number;
	const char baseChars[] = "0123456789ABCDEF";
	char temp[33];
	temp[32] = 0;
	int i = 31;
	do
	{
		temp[i--] = baseChars[number % base];
		number /= base;
	} while (number > 0);
	if (sign)
		temp[i--] = '-';
	for (; i > 31 - minCharacters; i--)
		temp[i] = ' ';
	print(&temp[i + 1]);
}

void VGA::print(unsigned long number, int base = 10, int minCharacters = 1)
{
	if(minCharacters < 1)
		minCharacters = 1;
	const char baseChars[] = "0123456789ABCDEF";
	char temp[33];
	temp[32] = 0;
	int i = 31;
	do
	{
		temp[i--] = baseChars[number % base];
		number /= base;
	} while (number > 0);
	for (; i > 31 - minCharacters; i--)
		temp[i] = ' ';
	print(&temp[i + 1]);
}	

void VGA::println(long number, int base = 10, int minCharacters = 1)
{
	print(number, base, minCharacters); print("\n");
}

void VGA::println(unsigned long number, int base = 10, int minCharacters = 1)
{
	print(number, base, minCharacters); print("\n");
}

void VGA::print(int number, int base = 10, int minCharacters = 1)
{
	print(long(number), base, minCharacters);
}

void VGA::println(int number, int base = 10, int minCharacters = 1)
{
	println(long(number), base, minCharacters);
}

void VGA::print(unsigned int number, int base = 10, int minCharacters = 1)
{
	print((unsigned long)(number), base, minCharacters);
}

void VGA::println(unsigned int number, int base = 10, int minCharacters = 1)
{
	println((unsigned long)(number), base, minCharacters);
}

void VGA::print(short number, int base = 10, int minCharacters = 1)
{
	print(long(number), base, minCharacters);
}

void VGA::println(short number, int base = 10, int minCharacters = 1)
{
	println(long(number), base, minCharacters);
}

void VGA::print(unsigned short number, int base = 10, int minCharacters = 1)
{
	print(long(number), base, minCharacters);
}

void VGA::println(unsigned short number, int base = 10, int minCharacters = 1)
{
	println(long(number), base, minCharacters);
}

void VGA::print(unsigned char number, int base = 10, int minCharacters = 1)
{
	print(long(number), base, minCharacters);
}

void VGA::println(unsigned char number, int base = 10, int minCharacters = 1)
{
	println(long(number), base, minCharacters);
}

void VGA::println()
{
	print("\n");
}

void VGA::print(double number, int fractionalDigits = 2, int minCharacters = 1)
{
	long p = long(pow(10, fractionalDigits));
	long long n = (double(number) * p + 0.5f);
	print(long(n / p), 10, minCharacters - 1 - fractionalDigits);
	if(fractionalDigits)
	{
		print(".");
		for(int i = 0; i < fractionalDigits; i++)
		{
			p /= 10;
			print(long(n / p) % 10);
		}
	}
}

void VGA::println(double number, int fractionalDigits = 2, int minCharacters = 1)
{
	print(number, fractionalDigits, minCharacters); 
	print("\n");
}
	
	
// графические примитивы
// graphic primitives

// это для заполненного треугольника подготовка *************

#define fixed int
#define round(x) floor(x + 0.5)
#define roundf(x) floor(x + 0.5f)


inline void SSswap(int &a, int &b)   //я сделал еще один SWAP (назвал SSswap)
{
      int t;
      t = a;
      a = b;
      b = t;
}

// представляем целое число в формате чисел с фиксированной точкой
inline fixed int_to_fixed(int value)
{
	return (value << 16);
}

// целая часть числа с фиксированной точкой
inline int fixed_to_int(fixed value)
{
	if (value < 0) return (value >> 16 - 1);
	if (value >= 0) return (value >> 16);
}

// округление до ближайшего целого
inline int round_fixed(fixed value)
{
	return fixed_to_int(value + (1 << 15));
}

// представляем число с плавающей точкой в формате чисел с фиксированной точкой
// здесь происходят большие потери точности
inline fixed double_to_fixed(double value)
{
	return round(value * (65536.0));
}

inline fixed float_to_fixed(float value)
{
	return roundf(value * (65536.0f));
}

// записываем отношение (a / b) в формате чисел с фиксированной точкой
inline fixed frac_to_fixed(int a, int b)
{
	return (a << 16) / b;
}

// окончание кода подготовки для заполненного треугольника *************

void VGA::xLineFast(int x0, int x1, int y, int rgb)
{
	if (y < 0 || y >= yres)
		return;
	if (x0 > x1)
	{
		int xb = x0;
		x0 = x1;
		x1 = xb;
	}
	if (x0 < 0)
		x0 = 0;
	if (x1 > xres)
		x1 = xres;
	for (int x = x0; x < x1; x++){			

	dotFast(x, y, rgb);

	}
}
	
	
	
void VGA::line(int x1, int y1, int x2, int y2, int rgb)
{
	int x, y, xe, ye;
	int dx = x2 - x1;
	int dy = y2 - y1;
	int dx1 = labs(dx);
	int dy1 = labs(dy);
	int px = 2 * dy1 - dx1;
	int py = 2 * dx1 - dy1;
	if (dy1 <= dx1)
	{
		if (dx >= 0)
		{
			x = x1;
			y = y1;
			xe = x2;
		}
		else
		{
			x = x2;
			y = y2;
			xe = x1;
		}
	dot(x, y, rgb);
			
		for (int i = 0; x < xe; i++)
		{
			x = x + 1;
			if (px < 0)
			{
				px = px + 2 * dy1;
			}
			else
			{
				if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
				{
					y = y + 1;
				}
				else
					{
						y = y - 1;
					}
					px = px + 2 * (dy1 - dx1);
				}
	dot(x, y, rgb);
				
			}
		}
		else
		{
			if (dy >= 0)
			{
				x = x1;
				y = y1;
				ye = y2;
			}
			else
			{
				x = x2;
				y = y2;
				ye = y1;
			}
	dot(x, y, rgb);
			
			for (int i = 0; y < ye; i++)
			{
				y = y + 1;
				if (py <= 0)
				{
					py = py + 2 * dx1;
				}
				else
				{
					if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
					{
						x = x + 1;
					}
					else
					{
						x = x - 1;
					}
					py = py + 2 * (dx1 - dy1);
				}
	dot(x, y, rgb);
				
		}
	}
}
	
	
void VGA::tri(int x1, int y1, int x2, int y2, int x3, int y3, int rgb)    //рисуем треугольник (команда Бейсик TRI).
{		
	line(x1, y1, x2, y2, rgb);
	line(x2, y2, x3, y3, rgb);
	line(x3, y3, x1, y1, rgb);	
}

// ==============================начало отрисовки треугольника заполненного
	
void VGA::fillTri(int x1, int y1, int x2, int y2, int x3, int y3, int rgb) //Тут мы отрисовываем сам заполненный треугольник(команда Бейсик TRIC).
{
	// Упорядочиваем точки p1(x1, y1),
	// p2(x2, y2), p3(x3, y3)
  if (y2 < y1) {
    SSswap(y1, y2);
    SSswap(x1, x2);
  } // точки p1, p2 упорядочены
  if (y3 < y1) {
    SSswap(y1, y3);
    SSswap(x1, x3);
  } // точки p1, p3 упорядочены
	// теперь p1 самая верхняя
	// осталось упорядочить p2 и p3
  if (y2 > y3) {
    SSswap(y2, y3);
    SSswap(x2, x3);
  }

	// приращения по оси x для трёх сторон
	// треугольника
  fixed dx13 = 0, dx12 = 0, dx23 = 0;

	// вычисляем приращения
	// в случае, если ординаты двух точек
	// совпадают, приращения
	// полагаются равными нулю
  if (y3 != y1) {
    dx13 = int_to_fixed(x3 - x1);
    dx13 /= y3 - y1;
  }
  else
  {
    dx13 = 0;
  }

  if (y2 != y1) {
    dx12 = int_to_fixed(x2 - x1);
    dx12 /= (y2 - y1);
  }
  else
  {
    dx12 = 0;
  }

  if (y3 != y2) {
    dx23 = int_to_fixed(x3 - x2);
    dx23 /= (y3 - y2);
  }
  else
  {
    dx23 = 0;
  }

	// "рабочие точки"
	// изначально они находятся в верхней точке
  fixed wx1 = int_to_fixed(x1);
  fixed wx2 = wx1;

	// сохраняем приращение dx13 в другой переменной
  int _dx13 = dx13;

	// упорядочиваем приращения таким образом, чтобы
	// в процессе работы алгоритмы
	// точка wx1 была всегда левее wx2
  if (dx13 > dx12)
  {
    SSswap(dx13, dx12);
  }

	// растеризуем верхний полутреугольник
  for (int i = y1; i < y2; i++){
    // рисуем горизонтальную линию между рабочими точками
    for (int j = fixed_to_int(wx1); j <= fixed_to_int(wx2); j++){
	dot(j, i, rgb);
    }
    wx1 += dx13;
    wx2 += dx12;
  }

	// вырожденный случай, когда верхнего полутреугольника нет
	// надо разнести рабочие точки по оси x,
	// т.к. изначально они совпадают
  if (y1 == y2){
    wx1 = int_to_fixed(x1);
    wx2 = int_to_fixed(x2);
  }

	// упорядочиваем приращения
	// (используем сохраненное приращение)
  if (_dx13 < dx23)
  {
    SSswap(_dx13, dx23);
  }
  
	// растеризуем нижний полутреугольник
  for (int i = y2; i <= y3; i++){
    // рисуем горизонтальную линию между рабочими точками
    for (int j = fixed_to_int(wx1); j <= fixed_to_int(wx2); j++){
	dot(j, i, rgb);
    }
    wx1 += _dx13;
    wx2 += dx23;
  }
}

// ================================ конец отрисовки треугольника заполненного

void VGA::squircle(int center_x, int center_y, int a, int b, float n, int rgb) // суперЭллипс:координаты центра x,y и ширина и высота, коэф.формы, цвет. (команда Бейсик SQUIRCLE).
{
    n = n / 10;
	for(float x = 0; x < (a + a + 1); x = x + 1) {   //тут меняем шаг "x = x + 1", чем меньше шаг, тем выше четкость, но отрисовка дольше!!!

		float nReal = (float)1 / n;
		float y = pow(((1 - (pow(x, n) / pow(a, n))) * pow(b, n)), nReal);

		dot((center_x - x), (center_y + y), rgb); // левый верхний сектор
		dot((center_x + x), (center_y + y), rgb); // правый верхний сектор
		dot((center_x + x), (center_y - y), rgb); // правый нижний сектор
		dot((center_x - x), (center_y - y), rgb); // левый нижний сектор		
	}
}
	

void VGA::fillSquircle(int center_x, int center_y, int a, int b, float n, int rgb) // заполненная версия суперЭллипса: координаты центра x,y и ширина и высота, коэф.формы, цвет.(команда Бейсик SQUIRCLEC).
{
	n = n / 10;
	for(float x = 0; x < (a + a + 1); x = x + 1) {           //тут меняем шаг "x = x + 1", чем меньше шаг, тем выше четкость, но отрисовка дольше!!!

		float nReal = (float)1 / n;
		float y = pow(((1 - (pow(x, n) / pow(a, n))) * pow(b, n)), nReal);
                                                                                             // заполненная версия суперЭллипса.
		line((center_x - x), (center_y + y), (center_x - x), (center_y - y), rgb);					
		line((center_x + x), (center_y + y), (center_x + x), (center_y - y), rgb);
		
	}		
}

void VGA::fillRect(int x, int y, int w, int h, int rgb)
{
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > xres)
		w = xres - x;
	if (y + h > yres)
		h = yres - y;
	for (int j = y; j < y + h; j++){
		for (int i = x; i < x + w; i++){
	dot(i, j, rgb);
		}
	}
}

void VGA::rect(int x, int y, int w, int h, int rgb)
{
	fillRect(x, y, w, 1, rgb);
	fillRect(x, y, 1, h, rgb);
	fillRect(x, y + h - 1, w, 1, rgb);
	fillRect(x + w - 1, y, 1, h, rgb);
}

void VGA::circle(int x, int y, int r, int rgb)
{
	int oxr = r;
	for(int i = 0; i < r + 1; i++)
	{
		int xr = (int)sqrt(r * r - i * i);
		xLineFast(x - oxr, x - xr + 1, y + i, rgb);
		xLineFast(x + xr, x + oxr + 1, y + i, rgb);
		if(i) 
		{
		xLineFast(x - oxr, x - xr + 1, y - i, rgb);
		xLineFast(x + xr, x + oxr + 1, y - i, rgb);
		}
		oxr = xr;
	}
}

void VGA::fillCircle(int x, int y, int r, int rgb)
{
	for(int i = 0; i < r + 1; i++)
	{
		int xr = (int)sqrt(r * r - i * i);
		xLineFast(x - xr, x + xr + 1, y + i, rgb);
		if(i) 
		xLineFast(x - xr, x + xr + 1, y - i, rgb);
	}
}

void VGA::ellipse(int x, int y, int rx, int ry, int rgb)
{
	if(ry == 0)
		return;
	int oxr = rx;
	float f = float(rx) / ry;
	f *= f;
	for(int i = 0; i < ry + 1; i++)
	{
		float s = rx * rx - i * i * f;
		int xr = (int)sqrt(s <= 0 ? 0 : s);
		xLineFast(x - oxr, x - xr + 1, y + i, rgb);
		xLineFast(x + xr, x + oxr + 1, y + i, rgb);
		if(i) 
		{
		xLineFast(x - oxr, x - xr + 1, y - i, rgb);
		xLineFast(x + xr, x + oxr + 1, y - i, rgb);
		}
		oxr = xr;
	}
}

void VGA::fillEllipse(int x, int y, int rx, int ry, int rgb)
{
	if(ry == 0)
		return;
	float f = float(rx) / ry;
	f *= f;		
	for(int i = 0; i < ry + 1; i++)
	{
		float s = rx * rx - i * i * f;
		int xr = (int)sqrt(s <= 0 ? 0 : s);
		xLineFast(x - xr, x + xr + 1, y + i, rgb);
		if(i) 
		xLineFast(x - xr, x + xr + 1, y - i, rgb);
	}
}
	
void VGA::mouse(int x1, int y1)   // рисует курсор мыши
{		

	const int mouse12_19[] =
	{
	0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  
	0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  
	0, 65535, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,  
	0, 65535, 65535, 0, -1, -1, -1, -1, -1, -1, -1, -1,  
	0, 65535, 65535, 65535, 0, -1, -1, -1, -1, -1, -1, -1, 
	0, 65535, 65535, 65535, 65535, 0, -1, -1, -1, -1, -1, -1,  
	0, 65535, 65535, 65535, 65535, 65535, 0, -1, -1, -1, -1, -1,  
	0, 65535, 65535, 65535, 65535, 65535, 65535, 0, -1, -1, -1, -1,  
	0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 0, -1, -1, -1, 
	0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 0, -1, -1,  
	0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 0, -1, 
	0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 0, 
	0, 65535, 65535, 65535, 65535, 65535, 65535, 0, 0, 0, 0, 0, 
	0, 65535, 65535, 65535, 0, 65535, 65535, 0, -1, -1, -1, -1,  
	0, 65535, 65535, 0, -1, 0, 65535, 65535, 0, -1, -1, -1, 
	0, 65535, 0, -1, -1, 0, 65535, 65535, 0, -1, -1, -1, 
	0, 0, -1, -1, -1, -1, 0, 65535, 65535, 0, -1, -1, 
	-1, -1, -1, -1, -1, -1, 0, 65535, 65535, 0, -1, -1, 
	-1, -1, -1, -1, -1, -1, -1, 0, 0, -1, -1, -1,	
	};
	
	int rrr ;	
    for(int y = 0; y < 19; y++) {
    for(int x = 0; x < 12; x++) {		
		int col = mouse12_19[rrr] ;	  	
	 if (col >=0)			// -1 мы не отображаем пиксель. 
		{
		dot(x + x1, y + y1, col);	  
		rrr = rrr + 1;	
		}		 
	 else
		{
		rrr = rrr + 1;
		}	  
		}					
	}		
	rrr = 0;		
}
	

void VGA::scrollText(int dy) // тут можно сделать динамичный цвет
{
	for(int d = 0; d < dy; d++)
	{
	dmaBuffer->scrollTextDown16(backBuffer);		
	xLineFast(0, xres, yres - 1, 0);
	}
	dmaBuffer->attachBuffer(backBuffer);
	cursorY -= dy;
}
	
	
void VGA::get(int x, int y) // узнаем цвет пикселя по координатам
{		
	if(bits == 16)
	{
	println(dmaBuffer->getLineAddr16(y, backBuffer)[x]);
	}	
	else if(bits == 8)
	{
	println(dmaBuffer->getLineAddr8(y, backBuffer)[x]);
	}
}
	
void VGA::dotAdd(int x, int y, int rgb1) // в разработке
{
	if(x >= mode.hRes || y >= mode.vRes) return;
	
	if(bits == 16)
	{
	uint8_t r1 = (rgb1 & 0xF800) >> 11;
	uint8_t g1 = (rgb1 & 0x07E0) >> 5;
	uint8_t b1 = rgb1 & 0x001F;

	r1 = (r1 * 255) / 31;
	g1 = (g1 * 255) / 63;
	b1 = (b1 * 255) / 31;
		
	uint16_t rgb2 = dmaBuffer->getLineAddr16(y, backBuffer)[x];
		
	uint8_t r2 = (rgb2 & 0xF800) >> 11;
	uint8_t g2 = (rgb2 & 0x07E0) >> 5;
	uint8_t b2 = rgb2 & 0x001F;

	r2 = (r2 * 255) / 31;
	g2 = (g2 * 255) / 63;
	b2 = (b2 * 255) / 31;
	
	uint8_t newR = (r1 + r2) / 2;
	uint8_t newG = (g1 + g2) / 2;
	uint8_t newB = (b1 + b2) / 2;
	
	dmaBuffer->getLineAddr16(y, backBuffer)[x] = rgb(newR, newG, newB);	
	}
		
	else if(bits == 8)
	{
    uint8_t r1 = ((rgb1 >> 5) & 0x07) * 36;
    uint8_t g1 = ((rgb1 >> 2) & 0x07) * 36;
    uint8_t b1 = ((rgb1) & 0x03) * 85;

	uint8_t rgb2 = dmaBuffer->getLineAddr8(y, backBuffer)[x];
			
	uint8_t r2 = ((rgb2 >> 5) & 0x07) * 36;
	uint8_t g2 = ((rgb2 >> 2) & 0x07) * 36;
	uint8_t b2 = ((rgb2) & 0x03) * 85;
	
	uint8_t newR = (r1 + r2) / 2;
	uint8_t newG = (g1 + g2) / 2;
	uint8_t newB = (b1 + b2) / 2;
	
	dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb(newR, newG, newB);
	}	
}

	
void VGA::scrollWindow(int x, int y, int w, int h, int dx, int dy) // Цикличный скролинг выбранного окна на любые направления	
{ 	
	if(bits == 16)
	{
		if(dx < 0){
		for (int qqq = 0; qqq < abs(dx); qqq++)
		{
		for (int yyy = y; yyy < y + h; yyy++)
		{
		dmaBuffer->getLineAddr16(yyy, backBuffer)[x + w] = dmaBuffer->getLineAddr16(yyy, backBuffer)[x];
		}
		
		for (int i = x; i < x + w; i++)for (int j = y; j < y + h; j++) //скролит <<<
		{
		dmaBuffer->getLineAddr16(j, backBuffer)[i] = dmaBuffer->getLineAddr16(j, backBuffer)[i + 1];
		}
		}	
		}
		
		if(dx > 0){
		for (int qqq = 0; qqq < dx; qqq++)
		{
		for (int yyy = y; yyy < y + h; yyy++)
		{
		dmaBuffer->getLineAddr16(yyy, backBuffer)[x] = dmaBuffer->getLineAddr16(yyy, backBuffer)[x + w];
		}
		
		for (int i = x + w; i > x; i--)for (int j = y; j < y + h; j++) //скролит >>>
		{
		dmaBuffer->getLineAddr16(j, backBuffer)[i] = dmaBuffer->getLineAddr16(j, backBuffer)[i - 1];
		}
		}	
		}
		
		if(dy < 0){
		for (int qqq = 0; qqq < abs(dy); qqq++)
		{
		for (int xxx = x; xxx < x + w; xxx++)
		{ 					
		dmaBuffer->getLineAddr16(y, backBuffer)[xxx] = dmaBuffer->getLineAddr16(y + h, backBuffer)[xxx];	
		}		
		for (int i = x; i < x + w; i++)for (int j = y + h; j > y; j--)
		{
		dmaBuffer->getLineAddr16(j, backBuffer)[i] = dmaBuffer->getLineAddr16(j - 1, backBuffer)[i];
		}		
		}
		}
		
		if(dy > 0){
		for (int qqq = 0; qqq < dy; qqq++)
		{
		for (int xxx = x; xxx < x + w; xxx++)
		{
		dmaBuffer->getLineAddr16(y + h , backBuffer)[xxx] = dmaBuffer->getLineAddr16(y, backBuffer)[xxx];
		}
		for (int i = x; i < x + w; i++)for (int j = y; j < y + h; j++)
		{
		dmaBuffer->getLineAddr16(j, backBuffer)[i] = dmaBuffer->getLineAddr16(j + 1, backBuffer)[i];
		}	
		}
		}	
	}

	else if(bits == 8)
	{
		if(dx < 0){
		for (int qqq = 0; qqq < abs(dx); qqq++)
		{
		for (int yyy = y; yyy < y + h; yyy++)
		{
		dmaBuffer->getLineAddr8(yyy, 0)[x + w] = dmaBuffer->getLineAddr8(yyy, 0)[x];
		}
		
		for (int i = x; i < x + w; i++)for (int j = y; j < y + h; j++) //скролит <<<
		{
		dmaBuffer->getLineAddr8(j, 0)[i] = dmaBuffer->getLineAddr8(j, 0)[i + 1];
		}
		}	
		}
		
		if(dx > 0){
		for (int qqq = 0; qqq < dx; qqq++)
		{
		for (int yyy = y; yyy < y + h; yyy++)
		{
		dmaBuffer->getLineAddr8(yyy, 0)[x] = dmaBuffer->getLineAddr8(yyy, 0)[x + w];
		}
		
		for (int i = x + w; i > x; i--)for (int j = y; j < y + h; j++) //скролит >>>
		{
		dmaBuffer->getLineAddr8(j, 0)[i] = dmaBuffer->getLineAddr8(j, 0)[i - 1];
		}
		}	
		}
		
		if(dy < 0){
		for (int qqq = 0; qqq < abs(dy); qqq++)
		{
		for (int xxx = x; xxx < x + w; xxx++)
		{ 					
		dmaBuffer->getLineAddr8(y, 0)[xxx] = dmaBuffer->getLineAddr8(y + h, 0)[xxx];	
		}		
		for (int i = x; i < x + w; i++)for (int j = y + h; j > y; j--)
		{
		dmaBuffer->getLineAddr8(j, 0)[i] = dmaBuffer->getLineAddr8(j - 1, 0)[i];
		}		
		}
		}
		
		if(dy > 0){
		for (int qqq = 0; qqq < dy; qqq++)
		{
		for (int xxx = x; xxx < x + w; xxx++)
		{
		dmaBuffer->getLineAddr8(y + h , 0)[xxx] = dmaBuffer->getLineAddr8(y, 0)[xxx];
		}
		for (int i = x; i < x + w; i++)for (int j = y; j < y + h; j++)
		{
		dmaBuffer->getLineAddr8(j, 0)[i] = dmaBuffer->getLineAddr8(j + 1, 0)[i];
		}	
		}
		}	
	}
}
	
	
void VGA::scrollFastWindow(int dx, int dy) // тут можно сделать динамичный цвет
{			
	if(dy > 0)
	{
	for(int d = 0; d < dy; d++)
	{
	dmaBuffer->scrollFastDown(backBuffer);		
	}
	dmaBuffer->attachBuffer(backBuffer);
	}
	if(dy < 0)
	{
	for(int d = 0; d < -dy; d++)
	{
	dmaBuffer->scrollFastUp(backBuffer);		
	}
	dmaBuffer->attachBuffer(backBuffer);
	}
	if(dx != 0)
	{
	scrollWindow(0, 0, xres-1, yres-1, dx, 0);
	}		
}
	
	

	


