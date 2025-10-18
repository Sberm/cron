# A simple cron implementation

Compile and run
```sh
sh dev.sh
```

Build
```sh
sh build.sh
```

Run
```sh
$ ./cron -h

  Cron by Howard Chu

    -h: Print this message
    -f <crontab file>: Path of the crontab file (default: ~/.crontab.txt)
```

Run it as a daemon
```sh
nohup ./cron > /dev/null &
```
