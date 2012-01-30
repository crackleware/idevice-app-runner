idevice-app-runner: idevice-app-runner.c
	gcc -g -pthread $^ -o $@ -limobiledevice

install: idevice-app-runner
	install -D $^ $(DESTDIR)/usr/bin/$^ 
