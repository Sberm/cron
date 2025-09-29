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

/* stands for start, end, step */
typedef struct ses {
    int start;
    int end;
    int step;
    int range;
} ses;

typedef struct cron_set {
    ses minute;
    ses hour;
    ses day_of_month;
    ses month;
    ses day_of_week;
} cron_set;

/* if no next token, set tok to NULL, and return 0 */
static int get_next_tok(char **pos, int *len, char *tok, int tok_size)
{
    char *start, *end;
    char *tmp_pos = NULL;
    size_t num;

    if (pos == NULL || len == NULL || tok == NULL) {
        pr_err("Some of get_next_tok's pointers are NULL\n");
        return 0;
    }

    tmp_pos = *pos;

    if (*len < 0) {
        pr_err("len can't be negative\n");
        return 0;
    }

    /* find the first occurence of non-space */
    for (start = tmp_pos; start < tmp_pos + *len && *start == ' '; ++start) {}
    if (start >= tmp_pos + *len) {
        tok = NULL; 
        return 0;
    }
    for (end = start; end < tmp_pos + *len && *end != ' '; ++end) {}
    num = min(tok_size, end - start);
    strncpy(tok, start, num);
    *len -= num + start - tmp_pos;
    *pos = end;
    return num;
}

static int check_minute(int num)
{
    if (num < 0 || num > 59) {
        pr_err("Bad minute(%d)\n", num);
        return -1;
    }
    pr_debug("Minute %d\n", num);
    return 0;
}

static int check_hour(int num)
{
    if (num < 0 || num > 23) {
        pr_err("Bad hour(%d)\n", num);
        return -1;
    }
    pr_debug("Hour %d\n", num);
    return 0;
}

static int check_day_of_month(int num)
{
    if (num < 1 || num > 31) {
        pr_err("Bad day of month(%d)\n", num);
        return -1;
    }
    pr_debug("Day of month %d\n", num);
    return 0;
}

static int check_month(int num)
{
    if (num < 1 || num > 12) {
        pr_err("Bad month(%d)\n", num);
        return -1;
    }
    pr_debug("Month %d\n", num);
    return 0;
}

static int check_day_of_week(int num)
{
    if (num < 1 || num > 12) {
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

    if (!crn_s)
        return 0;

    time(&raw_time);
    info = localtime(&raw_time);

    // TODO: is it the best order?
    /* order: month, day of week, day of month, hour, minute */
    if (crn_s->month.range) {
        if ((crn_s->month.start <= info->tm_mon || crn_s->month.start == -1) &&
            (info->tm_mon <= crn_s->month.end || crn_s->month.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->month.start == info->tm_mon || crn_s->month.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->day_of_week.range) {
        if ((crn_s->day_of_week.start <= info->tm_mon || crn_s->day_of_week.start == -1) &&
            (info->tm_mon <= crn_s->day_of_week.end || crn_s->day_of_week.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->day_of_week.start == info->tm_mon || crn_s->day_of_week.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->day_of_month.range) {
        if ((crn_s->day_of_month.start <= info->tm_mon || crn_s->day_of_month.start == -1) &&
            (info->tm_mon <= crn_s->day_of_month.end || crn_s->day_of_month.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->day_of_month.start == info->tm_mon || crn_s->day_of_month.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->hour.range) {
        if ((crn_s->hour.start <= info->tm_mon || crn_s->hour.start == -1) &&
            (info->tm_mon <= crn_s->hour.end || crn_s->hour.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->hour.start == info->tm_mon || crn_s->hour.start == -1) {
        } else {
            exec = 0;
        }
    }

    if (crn_s->minute.range) {
        if ((crn_s->minute.start <= info->tm_mon || crn_s->minute.start == -1) &&
            (info->tm_mon <= crn_s->minute.end || crn_s->minute.end == -1)) {
        } else {
            exec = 0;
        }
    } else {
        if (crn_s->minute.start == info->tm_mon || crn_s->minute.start == -1) {
        } else {
            exec = 0;
        }
    }

    return exec;
}

static void exec(char *comm)
{
    int pid = fork();

    if (pid == -1) {
        pr_err("Failed to fork\n");
        return;
    } else if (pid == 0) {
        pr_debug("Executing %s\n", comm);
        execve(comm, NULL, NULL);
        perror("execve");
    }
}

static int cron__sched(cron_set *crn_s, char *comm)
{
    if (crn_s == NULL)
        return -1;

    while (1) {
        if (cron__check(crn_s)) {
            exec(comm);
            sleep(10000);
        }
    }

    return 0;
}

/* caller will clear ses */
static int parse_ses(char *tok, ses *ses)
{
    char buf[NUM_LEN];
    char *st = tok;
    char *pos = tok;
    int err = 0;
    int tmp;

    if (tok == NULL || ses == NULL)
        return -1;

    pr_debug("parsing token %s\n", tok);

    /*
    (num | *)
    $num:
        (- | end)
        $-:
            (num | *)
        $num:
            (/ | end)
            $/:
                (step)
                $step: end
        $*:
            (/ | end)
            $/:
                (step)
                $step: end
    $*:
        (- | end)
        $-:
            (num)
                $num:
                    (/ | end)
                    $/:
                        (step)
                        $step: end
    */

    if (isdigit(*pos)) {
        char *end = NULL;
        for(end = pos; isdigit(*end) && *end; ++end) {}
        tmp = atoin(pos, end - pos);
        pos = end;
        if (!tmp) {
            pr_err("Can't be converted to integer\n");
            err = -1;
            goto out_parse;
        }
        ses->start = tmp;
        if (*pos == '-') {
            ++pos;
            ses->range = 1;
            if (isdigit(*pos)) {
                char *end = NULL;
                for(end = pos; isdigit(*end) && *end; ++end) {}
                tmp = atoin(pos, end - pos);
                pos = end;
                if (!tmp) {
                    pr_err("Can't be converted to integer\n");
                    err = -1;
                    goto out_parse;
                }
                ses->end = tmp;
                if (*pos == '/') {
                    ++pos;
                    if (isdigit(*pos)) { // step
                        char *end = NULL;
                        for(end = pos; isdigit(*end) && *end; ++end) {}
                        tmp = atoin(pos, end - pos);
                        pos = end;
                        if (!tmp) {
                            pr_err("Can't be converted to integer\n");
                            err = -1;
                            goto out_parse;
                        }
                        ses->step = tmp;
                    } else {
                        pr_err("\'/\' should be followed with a number\n");
                        err = -1;
                        goto out_parse;
                    }
                } else if (!*pos) {
                    goto out_parse;
                } else {
                    pr_err("Illegal character(%c)\n", *pos);
                    pr_debug("il cha 1\n");
                    err = -1;
                    goto out_parse;
                }
            } else if (*pos == '*') {
                ++pos;
                ses->end = -1;
                ses->range = 1;
                if (*pos == '/') {
                    ++pos;
                    if (isdigit(*pos)) { // step
                        char *end = NULL;
                        for(end = pos; isdigit(*end) && *end; ++end) {}
                        tmp = atoin(pos, end - pos);
                        pos = end;
                        if (!tmp) {
                            pr_err("Can't be converted to integer\n");
                            err = -1;
                            goto out_parse;
                        }
                        ses->step = tmp;
                        goto out_parse;
                    } else {
                        pr_err("\'/\' should be followed with a number\n");
                        err = -1;
                        goto out_parse;
                    }
                } else if (!*pos) {
                    goto out_parse;
                } else {
                    pr_err("Illegal character(%c)\n", *pos);
                    pr_debug("il cha 2\n");
                    err = -1;
                    goto out_parse;
                }
            } else {
                pr_err("Illegal character(%c)\n", *pos);
                pr_debug("il cha 3\n");
                err = -1;
                goto out_parse;
            }
        } else if (!*pos) {
            goto out_parse;
        } else {
            pr_err("Illegal character(%c)\n", *pos);
            pr_debug("il cha 4\n");
            err = -1;
            goto out_parse;
        }
    } else if (*pos == '*') {
        ++pos;
        ses->start = -1;
        if (*pos == '-') {
            ++pos;
            ses->range = 1;
            if (isdigit(*pos)) {
                char *end = NULL;
                for(end = pos; isdigit(*end) && *end; ++end) {}
                tmp = atoin(pos, end - pos);
                pos = end;
                if (!tmp) {
                    pr_err("Can't be converted to integer\n");
                    err = -1;
                    goto out_parse;
                }
                ses->end = tmp;
                if (*pos == '/') {
                    ++pos;
                    if (isdigit(*pos)) {
                        char *end = NULL;
                        for(end = pos; isdigit(*end) && *end; ++end) {}
                        tmp = atoin(pos, end - pos);
                        pos = end;
                        if (!tmp) {
                            pr_err("Can't be converted to integer\n");
                            err = -1;
                            goto out_parse;
                        }
                        ses->step = tmp;
                    } else {
                        pr_err("\'/\' should be followed with a number\n");
                        err = -1;
                        goto out_parse;
                    }
                } else if (!*pos) {
                    goto out_parse;
                } else {
                    pr_err("Illegal character(%c)\n", *pos);
                    pr_debug("il cha 5\n");
                }
            } else {
                pr_err("Illegal character(%c)\n", *pos);
                err = -1;
                goto out_parse;
            }
        } else if (!*pos) {
            goto out_parse;
        } else {
            pr_err("Illegal character(%c)\n", *pos);
            pr_debug("il cha 6\n");
            err = -1;
            goto out_parse;
        }
    } else {
        pr_err("Illegal character(%c)\n", *pos);
        pr_debug("il cha 7\n");
        err = -1;
    }
out_parse:

    return err;
}

static int parse(const char *vbuf)
{
    /* get the raw pointer */
    char *pos = __vec__at(vbuf, 0);
    int len = vec__len(vbuf);
    char tok[NUM_LEN];
    int cnt = 0;
    char comm[256];
    cron_set crn_s;

    memset(&crn_s, 0, sizeof(crn_s));
    memset(tok, 0, sizeof(tok));

    for (int idx = 0;
         idx < CRON_NUM && get_next_tok(&pos, &len, tok, sizeof(tok));
         ++idx, ++cnt, memset(tok, 0, sizeof(tok))) {
        int tmp;
        ses *ses_tmp;

        switch (idx) {
        case 0:
            ses_tmp = &crn_s.minute;
            tmp = parse_ses(tok, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 1:
            ses_tmp = &crn_s.hour;
            tmp = parse_ses(tok, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 2:
            ses_tmp = &crn_s.day_of_month;
            tmp = parse_ses(tok, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 3:
            ses_tmp = &crn_s.month;
            tmp = parse_ses(tok, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 4:
            ses_tmp = &crn_s.day_of_week;
            tmp = parse_ses(tok, ses_tmp);
            if (tmp)
                return tmp;
            break;
        default:
            break;
        }
        pr_debug("(ses) start %d end %d step %d range %d\n\n", ses_tmp->start, ses_tmp->end, ses_tmp->step, ses_tmp->range);
    }

    // TODO: replace with memchr
    for (;pos < (char *)__vec__at(vbuf, 0) + vec__len(vbuf) * vec__mem_size(vbuf) &&
          *pos == ' '; ++pos) {}

    if (strncpy(comm, pos,
               min(vec__len(vbuf) * vec__mem_size(vbuf), sizeof(comm))) == 0) {
        pr_err("Empty command\n");
        return -1;
    }

    cron__sched(&crn_s, comm);

    if (cnt < CRON_NUM) {
        pr_err("Only has %d numbers, needs to be %d\n", cnt, CRON_NUM);
        return -1;
    }
    return 0;
}

static int start()
{
    int err = 0;
    FILE *f = fopen("crontab.txt", "r");
    char *vbuf = read_line_v(f);

    if (f == NULL) {
        perror("Failed to open the crontab file");
        err = -1;
        goto out;
    }
    err = parse(vbuf);
    if (fclose(f) == EOF) {
        perror("Failed to close the crontab file");
        err = -1;
    }

    vec__free(vbuf);
out:
    return err;
}

int main()
{
    return start();
}
