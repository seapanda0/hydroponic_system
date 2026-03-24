#ifndef MAIN_LCD_PATTERNS_H_
#define MAIN_LCD_PATTERNS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7789.h"

void traceHeap(void);

TickType_t FillTest(TFT_t *dev, int width, int height);
TickType_t ColorBarTest(TFT_t *dev, int width, int height);
TickType_t ArrowTest(TFT_t *dev, FontxFile *fx, int width, int height);
TickType_t DirectionTest(TFT_t *dev, FontxFile *fx, int width, int height);
TickType_t HorizontalTest(TFT_t *dev, FontxFile *fx, int width, int height);
TickType_t VerticalTest(TFT_t *dev, FontxFile *fx, int width, int height);
TickType_t LineTest(TFT_t *dev, int width, int height);
TickType_t CircleTest(TFT_t *dev, int width, int height);
TickType_t RectAngleTest(TFT_t *dev, int width, int height);
TickType_t TriangleTest(TFT_t *dev, int width, int height);
TickType_t RoundRectTest(TFT_t *dev, int width, int height);
TickType_t FillRectTest(TFT_t *dev, int width, int height);
TickType_t ColorTest(TFT_t *dev, int width, int height);

void ST7789(void *pvParameters);

#endif /* MAIN_LCD_PATTERNS_H_ */