#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

enum GameMode {
	BASH, MAP
};

enum Mode {
	INSERT, COMMAND
};

const int MAX_X = 80;
const int MAX_Y = 20;

enum GameMode GM = BASH;

void clearAndReset() {
  // Reset all colors
  printf("\x1b[0m");
  // Move cursor to (0,0)
  printf("\x1b[H");
  // Clear the screen
  printf("\x1b[J");
}

int containerize() {
	int uid = getuid();
	int gid = getgid();

	if(unshare(CLONE_NEWUSER) != 0) {
		printf("Failed to unshare user namespace!\n");
		return -1;
	}

	int fd = open("/proc/self/setgroups", O_WRONLY);
	if(fd < 0) {
		printf("Failed to open setgroups!\n");
		return -1;
	}
	if(write(fd, "deny", 4) < 0) {
		printf("Failed to write setgroups! %d\n", errno);
		close(fd);
		return -1;
	}
	close(fd);

	char buf[100];
	snprintf(buf, sizeof(buf), "0 %d 1\n", uid);
	fd = open("/proc/self/uid_map", O_WRONLY);
	if(fd < 0) {
		printf("Failed to open uid_map!\n");
		return -1;
	}
	if(write(fd, buf, strlen(buf)) < 0) {
			printf("Failed to write uid_map!\n");
			close(fd);
			return -1;
	}
	close(fd);

	snprintf(buf, sizeof(buf), "0 %d 1\n", gid);
	fd = open("/proc/self/gid_map", O_WRONLY);
	if(fd < 0) {
		printf("Failed to open gid_map!\n");
		return -1;
	}
	if(write(fd, buf, strlen(buf)) < 0) {
			printf("Failed to write gid_map!\n");
			close(fd);
			return -1;
	}
	close(fd);

	if(unshare(CLONE_NEWNS) < 0) {
		printf("Failed to unshare mount namespace!");
		return -1;
	}
	
	// Make our directory structure in tmp
	mkdir("/tmp/bashgeonrt", 0700);
	mkdir("/tmp/bashgeonrt/bin", 0755);
	mkdir("/tmp/bashgeonrt/dev", 0755);
	mkdir("/tmp/bashgeonrt/dev/pts", 0755);
	mkdir("/tmp/bashgeonrt/entrance", 0700);
	mkdir("/tmp/bashgeonrt/home", 0700);
	mkdir("/tmp/bashgeonrt/lib", 0700);
	mkdir("/tmp/bashgeonrt/lib64", 0700);
	//mkdir("/tmp/bashgeonrt/proc", 0700);
	mkdir("/tmp/bashgeonrt/tmp", 0700);
	mkdir("/tmp/bashgeonrt/usr", 0700);
	mkdir("/tmp/bashgeonrt/usr/bin", 0700);
	mkdir("/tmp/bashgeonrt/usr/lib", 0700);

	if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
		printf("Mount root failed: %d\n", errno);
		return -1;
	}
	
	if(mount("/bin", "/tmp/bashgeonrt/bin", NULL, 
				MS_BIND | MS_REC | MS_RDONLY, NULL) < 0) {
		printf("Mount bin failed: %d\n", errno);
		return -1;
	}

	if(mount("/dev", "/tmp/bashgeonrt/dev", NULL,
				MS_BIND | MS_REC, NULL) < 0) {
		perror("mount dev");
		return -1;
	}

	if(mount("none", "/tmp/bashgeonrt/home", "tmpfs",
				0, "size=2M") < 0) {
		perror("mount_home");
		return -1;
	}

	if(mount("/lib", "/tmp/bashgeonrt/lib", NULL,
				MS_BIND | MS_REC | MS_RDONLY, NULL) < 0) {
		printf("Mount lib failed: %d\n", errno);
		return -1;
	}

	if(mount("/lib64", "/tmp/bashgeonrt/lib64", NULL,
				MS_BIND | MS_REC | MS_RDONLY, NULL)< 0) {
		printf("Mount lib64 failed: %d\n", errno);
		return -1;
	}

	/*if(mount("/proc", "/tmp/bashgeonrt/proc", "proc",
				MS_BIND | MS_REC, NULL) < 0) {
		printf("Mount proc failed: %d\n", errno);
		return -1;
	}*/

	if(mount("/tmp", "/tmp/bashgeonrt/tmp", "tmpfs",
				0, "size=2M") < 0) {
		perror("mount_tmp");
		return -1;
	}

	if(mount("/usr/bin", "/tmp/bashgeonrt/usr/bin", NULL, 
				MS_BIND | MS_REC | MS_RDONLY, NULL) < 0) {
		printf("Mount usr/bin failed: %d\n", errno);
		return -1;
	}

	if(mount("/usr/lib", "/tmp/bashgeonrt/usr/lib", NULL,
				MS_BIND | MS_REC | MS_RDONLY, NULL) < 0) {
		printf("Mount usr/lib failed: %d\n", errno);
		return -1;
	}

	if(mount("/tmp/bashgeonrt", "/tmp/bashgeonrt", NULL,
			MS_BIND | MS_REC, NULL) < 0) {
		printf("Mount tmprt failed: %d\n", errno);
		return -1;
	}
	
	if(chroot("/tmp/bashgeonrt") != 0) {
		printf("chroot failed :(, %d\n", errno);
		return -1;
	}
	chdir("/entrance");

	FILE *fp = fopen("/home/.bash_history", "w");
	if(fp == NULL) {
		perror("bashhistory");
		return -1;
	}
	fclose(fp);

	fp = fopen("/tmp/inject.sh", "w");
	if(fp == NULL) {
		perror("l1_inj");
		return -1;
	}
	fprintf(fp, "\n\
		export PS1='\\w $ '\n\
		export HISTFILE=/home/.bash_history\n\
		source() {\n\
			builtin source \"$@\"\n\
			export PS1='\\w $ '\n\
		}\n\
		readonly -f source ");
	fclose(fp);

	return 0;
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

void drawMap(const char tileChars[MAX_Y][MAX_X]) {
	for(int y=0; y<MAX_Y; y++) {
		printf("\x1b[%d;1H%s", y+1, &tileChars[y][0]);
	}
}

void handleMapInput(char tileChars[MAX_Y][MAX_X]) {
	enum Mode currMode = COMMAND;
	int playerX = 0;
	int playerY = 0;
	unsigned char action = '\0';
	char goalStr[] = "abc";

	while(1) {
		printf("\x1b[%d;%dH", playerY+1, playerX+1);
		action = getchar();
		if(currMode == COMMAND) {
			switch(action) {
				// Deletion
				/*case 'x':
					printf("\x1b[0K");
					for(int x=playerX; x<MAX_X-2 && tileChars[playerY][x] != '\0'; x++) {
						tileChars[playerY][x] = tileChars[playerY][x+1];
						printf("%c", tileChars[playerY][x]);
					}
					printf("\x1b[%d;%dH|", playerY+1, MAX_X);
					if(tileChars[playerY][playerX] == '\0' && playerX != 0) playerX--;
					printf("\x1b[%d;%dH", playerY+1, playerX+2);
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
					break;*/

				case '\x1b':
					GM = BASH;
					printf("\x1b[u");
					fflush(stdout);
					return;

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
		/*else {
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
					printf("\x1b[%d;%dH|", playerY+1, MAX_X);
					playerX++;
					printf("\x1b[%d;%dH", playerY+1, playerX+1);
					rightChar++;
					if(strcmp(&tileChars[0][0], goalStr) == 0) {
						printf("\x1b[7;2H\x1b[1mGOAL: \x1b[32m%s", goalStr);
						printf("\x1b[%d;%dH", playerY+1, 2);
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
		}*/
	}

	printf("\x1b[0m");
	printf("\x1b[2;2H\x1b[2KFantastic! I knew you could do it. Now, let's save this file and");
	printf("\x1b[3;2H\x1b[2Kget outta here with the following sequence:");
	printf("\x1b[4;2H\x1b[2K:wq");
	printf("\x1b[%d;%dH", playerY+12, playerX+2);
	
	while(action != ':') action = getchar();
	printf("\x1b[%d;1H:", MAX_Y);

	while(action != 'w') action = getchar();
	printf("\x1b[%d;2Hw", MAX_Y);

	while(action != 'q') action = getchar();
	printf("\x1b[%d;3Hq", MAX_Y);

	while(action != '\n') action = getchar();
}

int makePty(struct termios *raw, pid_t *pid) {
	// Set scrolling region
	printf("\x1b[21;45r");
	// Move the cursor to row 10 col 0
	printf("\x1b[21;1H");
	fflush(stdout);

  tcsetattr(STDIN_FILENO, TCSANOW, raw);

	int masterFd;
	*pid = forkpty(&masterFd, NULL, NULL, NULL);

	if(*pid < 0) {
		perror("forkpty");
		printf("error: %d\n", errno);
		return -1;
	}

	if(*pid == 0) {
		execlp("bash", "bash", "--rcfile", "/tmp/inject.sh", NULL);
		perror("execl");
		_exit(1);
	}

	return masterFd;
}

int handleBashInput(int masterFd) {
	// 30ms frame time
	struct timeval tv = {0, 30000};
	while(1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(masterFd, &fds);

		int maxFd = (masterFd > STDIN_FILENO ? masterFd : STDIN_FILENO);

		select(maxFd + 1, &fds, NULL, NULL, &tv);

		if(FD_ISSET(masterFd, &fds)) {
			char buffer[1024];
			int n = read(masterFd, buffer, sizeof(buffer));
			if(n <= 0) break;

			write(STDOUT_FILENO, buffer, n);
		}
		if(FD_ISSET(STDIN_FILENO, &fds)) {
			char c;
			if(read(STDIN_FILENO, &c, 1) > 0) {
				if(c == '\x1b') {
					GM = MAP;
					printf("\x1b[s");
					return 1;
				}
				write(masterFd, &c, 1);
			}
		}
	}

	return 0;
}

int main() {
  clearAndReset();
	printf("\x1b[=19h");

	int err = containerize();
	if(err != 0) {
		printf("Failed to create a \"safe\" execution environment!\n");
		return -1;
	}
	
  struct termios noEcho, withEcho, raw;
  tcgetattr(STDIN_FILENO, &noEcho);
  tcgetattr(STDIN_FILENO, &withEcho);
  noEcho.c_lflag &= ~(ICANON | ECHO);
	cfmakeraw(&raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);

	pid_t ptyPid;
	char map[MAX_Y][MAX_X];
	for(int y=0; y<MAX_Y; y++) {
		for(int x=0; x<MAX_X; x++) {
			map[y][x] = '\0';
		}
	}
	strncpy(&map[0][0], ".....", 5);
	strncpy(&map[1][0], "...>.", 5);
	strncpy(&map[2][0], ".....", 5);

	// Init Level
	drawMap(map);
	int ptyFd = makePty(&raw, &ptyPid);
	printf("\x1b[s");
	fflush(stdout);

	while(1) {
		//handle map input
		if(GM == MAP) {
			tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);
			handleMapInput(map);
		}
		//handle bash input
		if(GM == BASH) {
			tcsetattr(STDIN_FILENO, TCSANOW, &raw);
			int bashRet =  handleBashInput(ptyFd);
			if(bashRet == 0) break;
		}
	}

	waitpid(ptyPid, NULL, 0);

	tcsetattr(STDIN_FILENO, TCSANOW, &withEcho);

  tcsetattr(stdin->_fileno, TCSANOW, &withEcho);
  clearAndReset();
	return 0;
}
