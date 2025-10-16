// Lcd.a (Library)
// SPI1 @36MHz (PCLK2=72MHz)

#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define LIME        0x07E0  // 순수 녹색
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define VIOLET	    0xf81f
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define SILVER      0xC618  // 연한 회색
#define GRAY        0x8410  // 중간 회색
#define MAROON      0x8000
#define OLIVE       0x8400
#define GREEN       0x07E0  // 기본 녹색 (LIME과 동일)
#define PURPLE      0x8010
#define TEAL        0x0410
#define NAVY        0x0010
#define ORANGE      0xFD20
#define PINK        0xFE19
#define BROWN       0xA145

extern void Lcd_Init(int mode);
extern void Lcd_Set_Display_Mode(int mode);
extern void Lcd_Set_Cursor(unsigned short x, unsigned short y);
extern void Lcd_Set_Windows(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2);
extern void Lcd_Put_Pixel(unsigned short x, unsigned short y, unsigned short color);
extern void Lcd_Clr_Screen(void);
extern void Lcd_Draw_Back_Color(unsigned short color);
extern void Lcd_Write_Data_16Bit(unsigned short color);
extern void Lcd_Draw_Box(int xs, int ys, int w, int h, unsigned short Color);
