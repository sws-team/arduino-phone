#include <U8glib.h>

U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);  // I2C / TWI

void setup(void)
{
	u8g.setFont(u8g_font_unifontr);
}

void loop()
{
	u8g.firstPage();
	do {
		u8g.setColorIndex(1);//white
		u8g.setPrintPos(10, 15);
		u8g.print("Test");
		u8g.setPrintPos(50, 30);
		u8g.print("Hello");
		u8g.println();
	}
	while(u8g.nextPage());
}
