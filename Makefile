idevice-app-runner: idevice-app-runner.c
	gcc -g -pthread $^ -o $@ /usr/lib/libimobiledevice.so
