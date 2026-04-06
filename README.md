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

### Notes
This is a cron implementation with my own twist.

For example, `*-10`, `40-*` is legal here, though illegal in classic cron.
