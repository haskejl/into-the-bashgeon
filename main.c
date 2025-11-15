#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

const int MAX_X = 80;
const int MAX_Y = 40;

enum Mode {
	INSERT, COMMAND
};

void clearAndReset() {
  // Reset all colors
  printf("\x1b[0m");
  // Move cursor to (0,0)
  printf("\x1b[H");
  // Clear the screen
  printf("\x1b[J");
}

void makeBox(int minX, int minY, int maxX, int maxY) {
	for(int y=minY+1; y<maxY; y++) {
		printf("\x1b[%d;0H|", y);
		printf("\x1b[%d;%dH|", y, maxX);
	}
	for(int x=minX; x<=maxX; x++) {
		printf("\x1b[%d;%dH-", minY, x);
		printf("\x1b[%d;%dH-", maxY, x);
	}
}

void prologue(struct termios *withEcho, struct termios *noEcho) {
	enum Mode currMode = COMMAND;
	char tileChars[MAX_Y-10][MAX_X-1];
	for(int y=0; y<MAX_Y-10; y++) {
		for(int x=0; x<MAX_X-1; x++) {
			tileChars[y][x] = '\0';
		}
	}

  int playerX = 0;
  int playerY = 0;
  unsigned char action = '\0';
	unsigned char goalStr[] = "PS1='\\u \\W \\$ '";

	printf("\x1b[2;2HWelcome to the Bashgeon! I need some help, but only had a few seconds");
	printf("\x1b[3;2Hto salvage some commands.");
	printf("\x1b[4;2HTry using the keys I was able to salvage a,h,i,l,x,<esc> to produce the phrase.");

	makeBox(1, 6, MAX_X, 8);
	printf("\x1b[7;2H\x1b[1mGOAL: \x1b[22m%s", goalStr);

	printf("\x1b[10;2HEditing: .bashrc");

	makeBox(1, 11, MAX_X, MAX_Y-1);

	strncpy(&tileChars[0][0], "\\U V S N", strlen("\\U V S N"));
	printf("\x1b[12;2H%s", &tileChars[0][0]);

	printf("\x1b[%d;3H COMMAND ", MAX_Y-1);

  while(1) {
		printf("\x1b[%d;%dH", playerY+12, playerX+2);
		action = getchar();
		if(currMode == COMMAND) {
			switch(action) {
				// Deletion
				case 'x':
					printf("\x1b[0K");
					for(int x=playerX; x<MAX_X-2 && tileChars[playerY][x] != '\0'; x++) {
						tileChars[playerY][x] = tileChars[playerY][x+1];
						printf("%c", tileChars[playerY][x]);
					}
					printf("\x1b[%d;%dH|", playerY+12, MAX_X);
					if(tileChars[playerY][playerX] == '\0' && playerX != 0) playerX--;
					printf("\x1b[%d;%dH", playerY+12, playerX+2);
					break;
					
				// Enter insert mode
				case 'a':
					if(playerX < MAX_X-1 && tileChars[playerY][playerX] != '\0') {
						playerX++;
					}
					currMode = INSERT;
					printf("\x1b[%d;4HINSERT -", MAX_Y-1);
					break;
				case 'i':
					currMode = INSERT;
					printf("\x1b[%d;4HINSERT -", MAX_Y-1);
					break;

				// Movement
				case 'h':
					if(playerX > 0) playerX--;
					break;
				case 'j':
					if(playerY < MAX_Y-2 && tileChars[playerY+1][0] != '\0') {
						int tmpX = 0;
						while(tileChars[playerY+1][tmpX+1] != '\0') tmpX++;
						if(playerX > tmpX) playerX = tmpX;
						playerY++;
					}
					break;
				case 'k':
					if(playerY > 0) {
						int tmpX = 0;
						while(tileChars[playerY-1][tmpX+1] != '\0') tmpX++;
						if(playerX > tmpX) playerX = tmpX;
						playerY--;
					}
					break;
				case 'l':
					if(playerX < MAX_X-1 && tileChars[playerY][playerX+1] != '\0') playerX++;
			}
		}
		else {
			// Find rightmost character
			int rightChar = playerX;
			while(tileChars[playerY][rightChar] != '\0') rightChar++;

			// Type
			while(action != 0x1B) {
				if(rightChar < MAX_X-2 && action >= 0x20 && action <= 0x7E) {
					for(int x=rightChar+1; x>playerX; x--) {
						tileChars[playerY][x] = tileChars[playerY][x-1];
					}
					tileChars[playerY][playerX] = action;
					printf("\x1b[0K%s", &tileChars[playerY][playerX]);
					printf("\x1b[%d;%dH|", playerY+12, MAX_X);
					playerX++;
					printf("\x1b[%d;%dH", playerY+12, playerX+2);
					rightChar++;
					if(strcmp(&tileChars[0][0], goalStr) == 0) {
						printf("\x1b[7;2H\x1b[1mGOAL: \x1b[32m%s", goalStr);
						printf("\x1b[%d;%dH", playerY+12, 2);
						printf("%s", &tileChars[0][0]);
						break;
					}
				}
				action = getchar();
			}
			if(tileChars[playerY][playerX] == '\0' && playerX != 0) playerX--;

			// Enter Command Mode
			currMode = COMMAND;
			printf("\x1b[%d;4HCOMMAND ", MAX_Y-1);
		}
		if(strcmp(&tileChars[0][0], goalStr) == 0) {
			printf("\x1b[7;2H\x1b[1mGOAL: \x1b[32m%s", goalStr);
			printf("\x1b[%d;%dH", playerY+12, 2);
			printf("%s", &tileChars[0][0]);
			break;
		}
  }

	printf("\x1b[0m");
	printf("\x1b[2;2H\x1b[2KFantastic! I knew you could do it. Now, let's save this file and");
	printf("\x1b[3;2H\x1b[2Kget outta here with the following sequence:");
	printf("\x1b[4;2H\x1b[2K<esc>:wq");
	printf("\x1b[%d;%dH", playerY+12, playerX+2);
	
	while(action != ':') action = getchar();
	printf("\x1b[%d;1H:", MAX_Y);

	while(action != 'w') action = getchar();
	printf("\x1b[%d;2Hw", MAX_Y);

	while(action != 'q') action = getchar();
	printf("\x1b[%d;3Hq", MAX_Y);

	while(action != '\n') action = getchar();
}

int main() {
  clearAndReset();
	printf("\x1b[=19h");

  struct termios noEcho, withEcho;
  tcgetattr(stdin->_fileno, &noEcho);
  tcgetattr(stdin->_fileno, &withEcho);
  noEcho.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(stdin->_fileno, TCSANOW, &noEcho);

	// Prologue will be optional later
  prologue(&withEcho, &noEcho);
	clearAndReset();
	printf("Yay! You did it!\n");
	printf("user@localhost / $ ");
  tcsetattr(stdin->_fileno, TCSANOW, &withEcho);

	while(1) {
		char cmd[50] = "";
		fgets(cmd, 50, stdin);
		if(strcmp(cmd, "exit\n") == 0) break;
		system(cmd);
		printf("user@localhost / $ ");
	}

  tcsetattr(stdin->_fileno, TCSANOW, &withEcho);
  clearAndReset();
}
