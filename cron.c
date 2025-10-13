#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "util.h"
#include "file.h"
#include "vec.h"
#include "atoin.h"

#define CRON_NUM 5
#define NUM_LEN 32
#define MAX_ARG 16
#define ARG_LEN 32
#define COMM_LEN 256
#define ONE_SEC 1

/* stands for start, end, step */
typedef struct ses {
    int start;
    int end;
    int step;
    int range;
    char sched[61]; /* TODO: translate range to sched map */
} ses;

typedef struct cron_set {
    ses minute;
    ses hour;
    ses day_of_month;
    ses month;
    ses day_of_week;
} cron_set;

/* if no next token, set tok to NULL, and return 0 */
static int get_next_tok(char **pos, char *tok, int tok_size)
{
    char *start, *end;
    size_t num;

    if (!pos || !tok) {
        pr_err("Some of get_next_tok's pointers are NULL\n");
        return -1;
    }

    /* find the first occurence of non-space */
    for (start = *pos; *start && *start == ' '; ++start) {}
    /* move till the first space */
    for (end = start; *end && *end != ' '; ++end) {}

    num = min(tok_size, end - start);
    strncpy(tok, start, num);
    tok[min(num, tok_size-1)] = '\0';
    *pos = end;
    return num;
}

static int check_minute(int num)
{
    if (num < -1 || num > 59) {
        pr_err("Bad minute(%d)\n", num);
        return -1;
    }
    pr_debug("Minute %d\n", num);
    return 0;
}

static int check_hour(int num)
{
    if (num < -1 || num > 23) {
        pr_err("Bad hour(%d)\n", num);
        return -1;
    }
    pr_debug("Hour %d\n", num);
    return 0;
}

static int check_day_of_month(int num)
{
    if (num < -1 || num == 0 || num > 31) {
        pr_err("Bad day of month(%d)\n", num);
        return -1;
    }
    pr_debug("Day of month %d\n", num);
    return 0;
}

static int check_month(int num)
{
    if (num < -1 || num == 0 || num > 12) {
        pr_err("Bad month(%d)\n", num);
        return -1;
    }
    pr_debug("Month %d\n", num);
    return 0;
}

static int check_day_of_week(int num)
{
    if (num < -1 || num == 0 || num > 12) {
        pr_err("Bad day of week(%d)\n", num);
        return -1;
    }
    pr_debug("Day of week %d\n", num);
    return 0;
}

static int cron__check(cron_set *crn_s)
{
    /* minute, hour, day of month, month, day of week */
    time_t raw_time;
    struct tm *info;
    int exec = 1;
    static int prev_min = -1;
    static int prev_hour = -1;
    static int prev_day_of_month = -1;
    static int prev_month = -1;
    static int prev_day_of_week = -1;
    int min;
    int hour;
    int mday;
    int mon;
    int wday;

    if (!crn_s)
        return 0;

    time(&raw_time);
    info = localtime(&raw_time);

    min = info->tm_min;
    hour = info->tm_hour;
    mday = info->tm_mday;
    mon = info->tm_mon + 1;
    wday = info->tm_wday + 1;

    if (prev_min != -1 &&
        (min < prev_min + (crn_s->minute.step ? crn_s->minute.step : 1) ||
         hour < prev_hour + crn_s->hour.step ||
         mday < prev_day_of_month + crn_s->day_of_month.step ||
         mon < prev_month + crn_s->month.step ||
         wday < prev_day_of_week + crn_s->day_of_week.step ))
        return 0;

    prev_min = min;
    prev_hour = hour;
    prev_day_of_month = mday;
    prev_month = mon;
    prev_day_of_week = wday;

    // TODO: is it the best order?
    /* order: month, day of week, day of month, hour, minute */
    if (crn_s->month.range) {
        if ((crn_s->month.start <= mon || crn_s->month.start == -1) &&
            (mon <= crn_s->month.end || crn_s->month.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->month.start == mon || crn_s->month.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->day_of_week.range) {
        if ((crn_s->day_of_week.start <= wday || crn_s->day_of_week.start == -1) &&
            (wday <= crn_s->day_of_week.end || crn_s->day_of_week.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->day_of_week.start == wday || crn_s->day_of_week.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->day_of_month.range) {
        if ((crn_s->day_of_month.start <= mday || crn_s->day_of_month.start == -1) &&
            (mday <= crn_s->day_of_month.end || crn_s->day_of_month.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->day_of_month.start == mday || crn_s->day_of_month.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->hour.range) {
        if ((crn_s->hour.start <= hour || crn_s->hour.start == -1) &&
            (hour <= crn_s->hour.end || crn_s->hour.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->hour.start == hour || crn_s->hour.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->minute.range) {
        if ((crn_s->minute.start <= min || crn_s->minute.start == -1) &&
            (min <= crn_s->minute.end || crn_s->minute.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->minute.start == min || crn_s->minute.start == -1) {
        } else {
            exec = 0;
        }
    }

    return exec;
}

static int get_next_arg(char **pos, char *arg, int arg_size)
{
    /*
    // for every occurence of space, one need to consume all of them
    $root
        (" | char | ' ')
        $":
            (" | ANY OTHER THAN '"')
            $":
                (root | end)
            $ANY OTHER THAN '"':
                (")
                    $":
                        (end)
        $char:
            (' ' | end)
            $' ':
                (root)
        $' '
            (root)
    */
    char sep;
    char *end = NULL;

    if (!pos || !*pos || !**pos)
        return -1;

    /* allows multiple spaces, go to the first non-space character */
    end = *pos;
    while(*end && *end == ' ')
        ++end;
    if (*end)
        *pos = end;

    if (**pos == '"') {
        sep = '"';
        ++*pos;
    } else {
        sep = ' ';
    }

    if (**pos) {
        int siz = 0;
        end = *pos;

        while (*end && *end != sep)
            ++end;
        if (sep == '"' && !*end) /* hits the end without the matching " */
            return -1;

        siz = min(end - *pos, arg_size - 1);
        strncpy(arg, *pos, siz);
        arg[siz] = '\0';
        if (sep == '"')
            *pos = end + 1;
        else
            *pos = end;
        return 0;
    }
    return -1;
}

static void exec(char *comm_args)
{
    int pid;
    char *args[MAX_ARG] = { NULL }; /* not the safest */
    char arg[ARG_LEN] = { 0 };
    int idx = 0;
    char comm_strip[COMM_LEN] = { 0 };
    char *pos = comm_args;

    for (int i = 0; i < MAX_ARG; ++i) {
        args[i] = malloc(ARG_LEN);
        if (args[i] == NULL) {
            pr_err("Failed to allocate args vector\n");
            goto out_free;
        }
        memset(args[i], 0, ARG_LEN);
    }

    while(idx < MAX_ARG && !get_next_arg(&pos, arg, sizeof(arg))) {
        if (idx == 0) // command
            strncpy(comm_strip, arg, ARG_LEN);
        strncpy(args[idx], arg, ARG_LEN);
        pr_debug("arg[%d] = %s\n", idx, arg);
        memset(arg, 0, ARG_LEN);
        ++idx;
    }

    if (idx < MAX_ARG)
        args[idx] = NULL;
    else
        args[MAX_ARG - 1] = NULL;

    pid = fork();
    if (pid == -1) {
        pr_err("Failed to fork\n");
        goto out_free;
        return;
    } else if (pid == 0) {
        pr_debug("Executing comm %s\n", comm_strip);
        pr_debug("Arguments: ");
        for (int i = 0; i < idx; ++i) {
            if (i < idx - 1)
                pr_debug("%s, ", args[i]);
            else
                pr_debug("%s", args[i]);
        }
        pr_debug("\n");
        execvp(comm_strip, args);
        perror("execvp");
        goto out;
    }
out_free:
    for (int i = 0; i < MAX_ARG; ++i)
        free(args[i]);
out:
    return;
}

static int cron__sched(cron_set *crn_s, char *comm_args)
{
    if (crn_s == NULL)
        return -1;

    while (1) {
        if (cron__check(crn_s))
            exec(comm_args);
        sleep(ONE_SEC);
    }
    return 0;
}

static int parse_ses(char **pos, ses *ses);

static int parse_num_lhs(char **pos, ses *ses)
{
    char *end = NULL;

    // consumes the number
    for(end = *pos; isdigit(*end) && *end; ++end) {}
    int tmp = atoin(*pos, end - *pos);
    *pos = end;
    if (!tmp) {
        pr_err("Can't be converted to integer\n");
        return -1;
    }
    ses->start = tmp;
    if (**pos == '-') {
        ++*pos;
        ses->range = 1;
        if (isdigit(**pos)) {
            char *end = NULL;

            for(end = *pos; isdigit(*end) && *end; ++end) {}
            tmp = atoin(*pos, end - *pos);
            *pos = end;
            if (!tmp) {
                pr_err("Can't be converted to integer\n");
                return -1;
            }
            ses->end = tmp;
            if (**pos == '/') {
                ++*pos;
                if (isdigit(**pos)) { // step
                    char *end = NULL;
                    for(end = *pos; isdigit(*end) && *end; ++end) {}
                    tmp = atoin(*pos, end - *pos);
                    *pos = end;
                    if (!tmp) {
                        pr_err("Can't be converted to integer\n");
                        return -1;
                    }
                    ses->step = tmp;
                    return 0;
                } else {
                    pr_err("\'/\' should be followed with a number\n");
                    return -1;
                }
            } else if (!**pos) {
                return 0;
            } else if (**pos == ',') {
                ++*pos;
                parse_ses(pos, ses);
            } else {
                pr_err("Illegal character(%c)\n", **pos);
                pr_debug("il cha 1\n");
                return -1;
            }
        } else if (**pos == '*') {
            ++*pos;
            ses->end = -1;
            ses->range = 1;
            if (**pos == '/') {
                ++*pos;
                if (isdigit(**pos)) { // step
                    char *end = NULL;
                    for(end = *pos; isdigit(*end) && *end; ++end) {}
                    tmp = atoin(*pos, end - *pos);
                    *pos = end;
                    if (!tmp) {
                        pr_err("Can't be converted to integer\n");
                        return -1;
                    }
                    ses->step = tmp;
                    return 0;
                } else {
                    pr_err("\'/\' should be followed with a number\n");
                    return -1;
                }
            } else if (!**pos) {
                return -1;
            } else {
                pr_err("Illegal character(%c)\n", **pos);
                pr_debug("il cha 2\n");
                return -1;
            }
        } else {
            pr_err("Illegal character(%c)\n", **pos);
            pr_debug("il cha 3\n");
            return -1;
        }
    } else if (!**pos) {
        return 0;
    } else {
        pr_err("Illegal character(%c)\n", **pos);
        pr_debug("il cha 4\n");
        return -1;
    }
    return 0;
}

static int parse_asterisk_lhs(char **pos, ses *ses)
{
    ++*pos;
    ses->start = -1;
    if (**pos == '-') {
        ++*pos;
        ses->range = 1;
        if (isdigit(**pos)) {
            char *end = NULL;
            for(end = *pos; isdigit(*end) && *end; ++end) {}
            int tmp = atoin(*pos, end - *pos);
            *pos = end;
            if (!tmp) {
                pr_err("Can't be converted to integer\n");
                return -1;
            }
            ses->end = tmp;
            if (**pos == '/') {
                ++*pos;
                if (isdigit(**pos)) {
                    char *end = NULL;
                    for(end = *pos; isdigit(*end) && *end; ++end) {}
                    int tmp = atoin(*pos, end - *pos);
                    *pos = end;
                    if (!tmp) {
                        pr_err("Can't be converted to integer\n");
                        return -1;
                    }
                    ses->step = tmp;
                } else {
                    pr_err("\'/\' should be followed with a number\n");
                    return -1;
                }
            } else if (!**pos) {
                return 0;
            } else {
                pr_err("Illegal character(%c)\n", **pos);
                pr_debug("il cha 5\n");
                return -1;
            }
        } else {
            pr_err("Illegal character(%c)\n", **pos);
            return -1;
        }
    } else if (**pos == ',') {
        ++*pos;
        parse_ses(pos, ses);
    } else if (!**pos) {
        return 0;
    } else {
        pr_err("Illegal character(%c)\n", **pos);
        pr_debug("il cha 6\n");
        return -1;
    }
    return 0;
}

/* caller will clear ses */
static int parse_ses(char **pos, ses *ses)
{
    int err = 0;

    if (pos == NULL || ses == NULL)
        return -1;

    /*
    $root:
        (num | *)
        $num:
            (- | , | end)
            $-:
                (num | *)
                $num:
                    (/ | end)
                    $/:
                        (step)
                        $step:
                            (end)
            $,:
                (root)
        $*:
            (- | , | end)
            $-:
                (num)
                $num:
                    (/ | end)
                    $/:
                        (step)
                        $step: end
            $,:
                (root)
    */

    if (isdigit(**pos)) {
        err = parse_num_lhs(pos, ses);
        if (err)
            return err;
    } else if (**pos == '*') {
        err = parse_asterisk_lhs(pos, ses);
        if (err)
            return err;
    } else {
        pr_err("Illegal character(%c)\n", **pos);
        pr_debug("il cha 7\n");
        return -1;
    }
    return 0;
}

static int parse(const char *vbuf)
{
    /* get the raw pointer */
    char *pos = __vec__at(vbuf, 0);
    int len = vec__len(vbuf);
    char tok[NUM_LEN];
    int cnt = 0;
    char comm_args[COMM_LEN];
    cron_set crn_s;

    memset(&crn_s, 0, sizeof(crn_s));
    memset(tok, 0, sizeof(tok));

    for (int idx = 0;
         idx < CRON_NUM && get_next_tok(&pos, tok, sizeof(tok));
         ++idx, ++cnt, memset(tok, 0, sizeof(tok))) {
        int tmp;
        ses *ses_tmp;
        char *pos = tok;

        switch (idx) {
        case 0:
            ses_tmp = &crn_s.minute;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            if (check_minute(ses_tmp->start) || (ses_tmp->range && check_minute(ses_tmp->end))) {
                pr_err("Illegal minute(start %d end %d step %d)\n", ses_tmp->start, ses_tmp->end, ses_tmp->step);
                return -1;
            }
            break;
        case 1:
            ses_tmp = &crn_s.hour;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            if (check_hour(ses_tmp->start) || (ses_tmp->range && check_hour(ses_tmp->end))) {
                pr_err("Illegal hour(start %d end %d step %d)\n", ses_tmp->start, ses_tmp->end, ses_tmp->step);
                return -1;
            }
            break;
        case 2:
            ses_tmp = &crn_s.day_of_month;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            if (check_day_of_month(ses_tmp->start) || (ses_tmp->range && check_day_of_month(ses_tmp->end))) {
                pr_err("Illegal day of month(start %d end %d step %d)\n", ses_tmp->start, ses_tmp->end, ses_tmp->step);
                return -1;
            }
            break;
        case 3:
            ses_tmp = &crn_s.month;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            if (check_month(ses_tmp->start) || (ses_tmp->range && check_month(ses_tmp->end))) {
                pr_err("Illegal month(start %d end %d step %d)\n", ses_tmp->start, ses_tmp->end, ses_tmp->step);
                return -1;
            }
            break;
        case 4:
            ses_tmp = &crn_s.day_of_week;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            if (check_day_of_week(ses_tmp->start) || ses_tmp->range && check_day_of_week(ses_tmp->end)) {
                pr_err("Illegal day of week(start %d end %d step %d)\n", ses_tmp->start, ses_tmp->end, ses_tmp->step);
                return -1;
            }
            break;
        default:
            break;
        }
        pr_debug("(ses) start %d end %d step %d range %d\n\n", ses_tmp->start, ses_tmp->end, ses_tmp->step, ses_tmp->range);
    }

    // TODO: replace with memchr
    // go to the first non-space
    for (;pos < (char *)__vec__at(vbuf, 0) + vec__len(vbuf) * vec__mem_size(vbuf) &&
          *pos == ' '; ++pos) {}

    /* copy the rest of the string to comm_args */
    if (strncpy(comm_args, pos, min(vec__len(vbuf) * vec__mem_size(vbuf), sizeof(comm_args))) == 0) {
        pr_err("Empty command\n");
        return -1;
    }
    pr_debug("comm_args: %s\n", comm_args);

    cron__sched(&crn_s, comm_args);

    if (cnt < CRON_NUM) {
        pr_err("Only has %d numbers, needs to be %d\n", cnt, CRON_NUM);
        return -1;
    }
    return 0;
}

static int start()
{
    int err = 0;
    FILE *f;
    char *vbuf;

    f = fopen("crontab.txt", "r");
    if (!f) {
        perror("Failed to open the crontab file");
        err = -1;
        goto out;
    }

    vbuf = vec__new(sizeof(char));
    if (!vbuf) {
        err = -1;
        goto out_free_fd;
    }

    if (read_line_v(f, vbuf)) {
        err = -1;
        goto out_free;
    }

    err = parse(vbuf);

out_free:
    vec__free(vbuf);
out_free_fd:
    if (fclose(f) == EOF) {
        perror("Failed to close the crontab file");
        err = -1;
    }
out:
    return err;
}

int main()
{
    return start();
}
