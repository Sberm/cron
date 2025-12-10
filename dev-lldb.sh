gcc -g -DDEBUG atoin.c vec.c file.c cron.c -o cron_debug
lldb -- cron_debug -f ./crontab.txt
