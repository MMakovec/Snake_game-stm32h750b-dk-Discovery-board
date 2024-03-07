/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SNAKE_H
#define __SNAKE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#include <stdlib.h>
#include <time.h>

#define LTDC_WIDTH      480
#define LTDC_HEIGHT     272
#define LTDC_PIXELCOUNT (LTDC_WIDTH*LTDC_HEIGHT)
#define LEFT 1
#define RIGHT 2
#define UP 3
#define DOWN 4
#define FRUIT_COL 0xF80F
#define SNAKE_WIDTH 10
#define FIELD_WIDTH 48
#define FIELD_HEIGHT 27
#define MAX_SPEED 17



// linked list
typedef struct Node{
	int x;
	int y;
	struct Node* next;
} Node;


extern uint8_t direction;
extern uint8_t running;
extern uint8_t start;
extern uint16_t score;
extern uint8_t snake_running;
static unsigned int xorshift_state;


extern void draw_square(uint16_t col, uint16_t dims, uint16_t* buffer, uint16_t posx, uint16_t posy);
void snake(uint16_t *framebuffer, Node** head);
void insertionAtBegin(Node** head, uint8_t* fruit, uint8_t fr_x, uint8_t fr_y);
void drawLinkedlist(Node* head, uint16_t* framebuffer);
void deleteFromEnd(Node** head);
void freeList(Node* head);
void make_snake(Node* head);
uint8_t searchNode(struct Node** head, int key1, int key2);
unsigned int xorshift32(void);
int random_range(int min, int max);





#ifdef __cplusplus
}
#endif

#endif
