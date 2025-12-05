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
	mkdir("/tmp/bashgeonrt/bin", 0700);
	mkdir("/tmp/bashgeonrt/dev", 0755);
	mkdir("/tmp/bashgeonrt/dev/pts", 0755);
	mkdir("/tmp/bashgeonrt/home", 0700);
	mkdir("/tmp/bashgeonrt/lib", 0700);
	mkdir("/tmp/bashgeonrt/lib64", 0700);
	//mkdir("/tmp/bashgeonrt/proc", 0700);
	mkdir("/tmp/bashgeonrt/tmp", 0700);
	mkdir("/tmp/bashgeonrt/usr", 0700);
	mkdir("/tmp/bashgeonrt/usr/bin", 0700);
	mkdir("/tmp/bashgeonrt/usr/lib", 0700);

	fd = open("/tmp/bashgeonrt/dev/ptmx", O_CREAT);
	close(fd);
	chmod("/tmp/bashgeonrt/dev/ptmx", 0666);
	
	fd = open("/tmp/bashgeonrt/dev/null", O_CREAT);
	if(fd < 0) {
		printf("Failed to create /dev/null, %d\n", errno);
		return -1;
	}
	close(fd);
	chmod("/tmp/bashgeonrt/dev/null", 0666);

	if(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
		printf("Mount root failed: %d\n", errno);
		return -1;
	}
	
	if(mount("/bin", "/tmp/bashgeonrt/bin", NULL, 
				MS_BIND | MS_REC | MS_RDONLY, NULL) < 0) {
		printf("Mount bin failed: %d\n", errno);
		return -1;
	}

	/*if(mount("/dev/ptmx", "/tmp/bashgeonrt/dev/ptmx", NULL,
				MS_BIND, NULL) < 0) {
		perror("bind ptmx");
		return -1;
	}

	if(mount("devpts", "/tmp/bashgeonrt/dev/pts", "devpts",
				0, "newinstance,ptmxmode=0666,mode=620") < 0) {
		perror("devpts");
		return -1;
	}*/
	if(mount("/dev", "/tmp/bashgeonrt/dev", NULL,
				MS_BIND | MS_REC, NULL) < 0) {
		perror("mount dev");
		return -1;
	}

	if(mount("/dev/null", "/tmp/bashgeonrt/dev/null", NULL,
				MS_BIND, NULL) < 0) {
		printf("Failed to mount /dev/null\n");
		return -1;
	}

	if(mount("/home", "/tmp/bashgeonrt/home", "tmpfs",
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
	chdir("/");

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
	unsigned char goalStr[] = "PS1='\\w \\$ '";

	printf("\x1b[2;2HWelcome to the Bashgeon! I need some help, but only had a few seconds");
	printf("\x1b[3;2Hto salvage some commands.");
	printf("\x1b[4;2HTry using the keys I was able to salvage a,h,i,l,x,<esc> to produce the phrase.");

	makeBox(1, 6, MAX_X, 8);
	printf("\x1b[7;2H\x1b[1mGOAL: \x1b[22m%s", goalStr);

	printf("\x1b[10;2HEditing: .bashrc");

	makeBox(1, 11, MAX_X, MAX_Y-1);

	strncpy(&tileChars[0][0], "PS1='\\W'", strlen("PS1='\\W'"));
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

int levelOne(struct termios *raw) {
	FILE *fp;
	fp = fopen("/home/.bashrc", "w");
	if(fp == NULL) {
		perror("bashrc");
		return -1;
	}
	char ps1[] = "PS1='\\w \\$ '\necho 'hey'\n\
								echo \"PS1='\\w \\$ '\">/home/.bashrc";
	
	fputs(ps1, fp);
	fclose(fp);

	fp = fopen("/tmp/inject.sh", "w");
	if(fp == NULL) {
		perror("l1_inj");
		return -1;
	}
	fprintf(fp, "cd /home");
	fclose(fp);
	
	printf("Congrats, you made it. It's kinda hard to tell where you are though.\n");
	printf("Remember that file you were working on. It's here somewhere.\n");
	printf("Use the command we learned earlier to List the files.\n");
	printf("Hint: Try using the flags -l -a, and -A (combined -la or -lA).\n");
	printf("\n");
	printf("When you find it, try source [filename].\n\n");

  tcsetattr(STDIN_FILENO, TCSANOW, raw);

	int masterFd;
	pid_t pid = forkpty(&masterFd, NULL, NULL, NULL);

	if(pid < 0) {
		perror("forkpty");
		printf("error: %d\n", errno);
		return -1;
	}

	if(pid == 0) {
		execlp("bash", "bash", "--rcfile", "/tmp/inject.sh", NULL);
		perror("execl");
		_exit(1);
	}

	while(1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(masterFd, &fds);

		int maxFd = (masterFd > STDIN_FILENO ? masterFd : STDIN_FILENO);

		select(maxFd + 1, &fds, NULL, NULL, NULL);

		if(FD_ISSET(masterFd, &fds)) {
			char buffer[1024];
			int n = read(masterFd, buffer, sizeof(buffer));
			if(n <= 0) break;

			write(STDOUT_FILENO, buffer, n);
		}
		if(FD_ISSET(STDIN_FILENO, &fds)) {
			char c;
			if(read(STDIN_FILENO, &c, 1) > 0)
				write(masterFd, &c, 1);
		}
	}

	waitpid(pid, NULL, 0);

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

	// Prologue will be optional later
  //prologue(&withEcho, &noEcho);
	clearAndReset();

	levelOne(&raw);
	
	clearAndReset();
	tcsetattr(STDIN_FILENO, TCSANOW, &withEcho);
	printf("I knew you could do it! Welcome to The Construct.\n");
	printf("Try using ls to look at what's here.\n");

	/*printf("user@localhost / $ ");
	while(1) {
		char cmd[50] = "";
		fgets(cmd, 50, stdin);
		if(strcmp(cmd, "exit\n") == 0) break;
		system(cmd);
		printf("user@localhost / $ ");
	}*/

  tcsetattr(stdin->_fileno, TCSANOW, &withEcho);
  clearAndReset();
	return 0;
}
