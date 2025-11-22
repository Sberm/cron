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
This is a cron implementation with my own twist. I was too lazy to read the cron specification details but I'll briefly mention the heuristics I used here.

When using step (e.g. `1-30/2`), my cron counts steps starting from the execution of the program. When it reaches a start of a range, it resets and starts counting from that start.

`*-10`, `40-*` is legal here, though illegal in classic cron.
