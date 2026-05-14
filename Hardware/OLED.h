#ifndef __OLED_H
#define __OLED_H

void OLED_Init(void);
void OLED_Clear(void);
void OLED_SetCursor(uint8_t Y, uint8_t X);
void OLED_WriteData(uint8_t Data);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowCharacter(uint8_t Line, uint8_t Column, short num);

void OLED_ShowChar_Reverse(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString_Reverse(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum_Reverse(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum_Reverse(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum_Reverse(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum_Reverse(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowCharacter_Reverse(uint8_t Line, uint8_t Column, short num);
void OLED_ReverseLine(uint8_t Line);
void OLED_ReverseArea(uint8_t Line, uint8_t Column, uint8_t Width, uint8_t Height);

#endif
