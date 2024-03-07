#include "snake.h"



// Function to seed the xorshift_state based on current time
void xorshift_seed(void) {
    // Use time as a seed
    unsigned int seed = (unsigned int)time(NULL);

    // Ensure the seed is not zero
    xorshift_state = (seed != 0) ? seed : 1;
}


// Custom xorshift-based random number generator
unsigned int xorshift32(void) {
    xorshift_state ^= (xorshift_state << 13);
    xorshift_state ^= (xorshift_state >> 17);
    xorshift_state ^= (xorshift_state << 5);
    return xorshift_state;
}

// Function to get a random number in a specific range [min, max]
int random_range(int min, int max) {
    // Ensure min <= max
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }

    // Calculate the range size and get a random number in that range
    unsigned int range_size = (unsigned int)(max - min + 1);
    unsigned int random_value = xorshift32();
    return (int)(random_value % range_size) + min;
}

// linked list
void insertionAtBegin(Node** head, uint8_t* fruit, uint8_t fr_x, uint8_t fr_y){
  Node* new = (Node*)malloc(sizeof(Node));
  // move head
  switch(direction){
	case UP:
		new->x = (*head)->x;
		new->y = (*head)->y-1;
		break;
	case LEFT:
		new->x = (*head)->x - 1;
		new->y = (*head)->y;
		break;
	case DOWN:
		new->x = (*head)->x;
		new->y = (*head)->y + 1;
		break;
	case RIGHT:
		new->x = (*head)->x + 1;
		new->y = (*head)->y;
		break;
	default:
		new->x = (*head)->x;
		new->y = (*head)->y;
  }

  // check for boundaries

  // pass-through
  if(new->x > FIELD_WIDTH-1) new->x = 0;
  else if(new->x < 0) new->x = FIELD_WIDTH-1;

  if(new->y > FIELD_HEIGHT-1) new->y = 0;
  else if(new->y < 0) new->y = FIELD_HEIGHT-1;

  // wall collision

  // snake collision
  if(searchNode(head, new->x, new->y)){
	  direction = RIGHT;
	  running = 0;
	  free(new); // Free the allocated memory before returning
  }

  // fruit
  if(new->x == fr_x && new->y == fr_y){
	  *fruit = 1;
	  score++;
  }

  new->next = *head;
  *head = new;
}

void drawLinkedlist(Node* head, uint16_t* framebuffer) {
  while (head != NULL) {
	draw_square(0x0000,SNAKE_WIDTH, framebuffer, head->x*SNAKE_WIDTH,head->y*SNAKE_WIDTH);
    head = head->next;
  }
}

void deleteFromEnd(Node** head) {
    if (*head == NULL || (*head)->next == NULL) {
        // Empty list or list with only one element
        return;
    }

    Node* current = *head;
    Node* previous = NULL;

    while (current->next != NULL) {
        previous = current;
        current = current->next;
    }

    free(current);

    // Update the previous node to be the new end of the list
    if (previous != NULL) {
        previous->next = NULL;
    } else {
        // If previous is NULL, it means we deleted the head node
        *head = NULL;
    }
}

uint8_t searchNode(struct Node** head, int key1, int key2) {
  struct Node* current = *head;

  while (current != NULL) {
    if (current->x == key1 && current->y == key2) return 1;
      current = current->next;
  }
  return 0;
}

void freeList(Node* head)
{
   Node* tmp;
   while (head != NULL)
    {
       tmp = head;
       head = head->next;
       free(tmp);
    }
}

void make_snake(Node* head){
	  head->x = 20;
	  head->y = 13;
	  head->next = malloc(sizeof(Node));
	  head->next->x = 19;
	  head->next->y = 13;
	  head->next->next = malloc(sizeof(Node));
	  head->next->next->x = 18;
	  head->next->next->y = 13;
	  head->next->next->next = NULL;
}


void snake(uint16_t *framebuffer, Node** head){
	snake_running = 1;

	uint8_t fruit = 0;
	static uint8_t fr_x = 255;
	static uint8_t fr_y = 255;

	// clear framebuffer
	for(int i = 0; i < LTDC_WIDTH*LTDC_HEIGHT; i++){
		if(framebuffer[i] != FRUIT_COL) framebuffer[i] = 0xFFFF;
	}

	// move snake

	// HEAD
	insertionAtBegin(head, &fruit, fr_x, fr_y);

	// did we lose?
	if(!running){
		snake_running = 0;
		start = 0;
		score = 0;
		freeList(*head);
		return;
	}

	// did we eat the fruit?
	if(fruit){
		start = 0;
	}

	if(start<2){
		if(!start){
			//xorshift_seed(); // generate random seed
			// generate fruit
			do{
				//fr_x = random_range(0, 47);
				//fr_y = random_range(0, 26);
				fr_x = rand()%47;
				fr_y = rand()%26;
			}while(searchNode(head, fr_x, fr_y));
		}
		// draw in both framebuffers
		draw_square(FRUIT_COL,SNAKE_WIDTH, framebuffer, fr_x*SNAKE_WIDTH,fr_y*SNAKE_WIDTH);

		start++;
	}

	// TAIL
	if(!fruit){
		deleteFromEnd(head);
	}

	// DRAW
	drawLinkedlist(*head, framebuffer);

	snake_running = 0;
}


