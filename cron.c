#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#if defined(__APPLE__) || defined(__MACH__)
#include <limits.h> /* PATH_MAX */
#endif

#include "util.h"
#include "file.h"
#include "vec.h"
#include "atoin.h"

#define CRON_NUM 5
#define NUM_LEN 32
#define MAX_ARG 128
#define ARG_LEN 256
#define COMM_LEN 1024
#define ONE_SEC 1
#define MAX_SCHED 61

#define MIN_MINUTE 0
#define MAX_MINUTE 59
#define MIN_HOUR 0
#define MAX_HOUR 23
#define MIN_DAY_OF_MONTH 1
#define MAX_DAY_OF_MONTH 31
#define MIN_MONTH 1
#define MAX_MONTH 12
#define MIN_DAY_OF_WEEK 1
#define MAX_DAY_OF_WEEK 7

#define HOME "HOME"
#define DEFAULT_CRONTAB_FMT "%s/.crontab.txt"

/* stands for start, end, step */
typedef struct Ses {
    /* 
     * start and end act as temporary variables for parsing,
     * do not represent the real start and end
     */
    int start;
    int end;
    int step;
    char sched[MAX_SCHED];
} Ses;

typedef struct cron_set {
    Ses minute;
    Ses hour;
    Ses day_of_month;
    Ses month;
    Ses day_of_week;
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
    if ((num >= MIN_MINUTE && num <= MAX_MINUTE) ||
         num == -1) {
        pr_debug("Minute %d\n", num);
        return 0;
    }
    pr_err("Bad minute(%d)\n", num);
    return -1;
}

static int check_hour(int num)
{
    if ((num >= MIN_HOUR && num <= MAX_HOUR) ||
         num == -1) {
        pr_debug("Hour %d\n", num);
        return 0;
    }
    pr_err("Bad hour(%d)\n", num);
    return -1;
}

static int check_day_of_month(int num)
{
    if ((num >= MIN_DAY_OF_MONTH && num <= MAX_DAY_OF_MONTH) ||
         num == -1) {
        pr_debug("Day of month %d\n", num);
        return 0;
    }
    pr_err("Bad day of month(%d)\n", num);
    return -1;
}

static int check_month(int num)
{
    if ((num >= MIN_MONTH && num <= MAX_MONTH) ||
         num == -1) {
        pr_debug("Month %d\n", num);
        return 0;
    }
    pr_err("Bad month(%d)\n", num);
    return -1;
}

static int check_day_of_week(int num)
{
    if ((num >= MIN_DAY_OF_WEEK && num <= MAX_DAY_OF_WEEK) ||
         num == -1) {
        pr_debug("Day of week %d\n", num);
        return 0;
    }
    pr_err("Bad day of week(%d)\n", num);
    return -1;
}

int check_bound(const int mn, const int mx, const int val)
{
    if (mn <= val && val <= mx)
        return 1;
    return 0;
}

static int cron__should_exec(cron_set *crn_s)
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

    if (check_bound(MIN_MONTH, MAX_MONTH, mon) &&
        !crn_s->month.sched[mon])
        return 0;

    if (check_bound(MIN_DAY_OF_WEEK, MAX_DAY_OF_WEEK, wday) &&
        !crn_s->day_of_week.sched[wday])
        return 0;

    if (check_bound(MIN_DAY_OF_MONTH, MAX_DAY_OF_MONTH, mday) &&
        !crn_s->day_of_month.sched[mday])
        return 0;

    if (check_bound(MIN_HOUR, MAX_HOUR, hour) &&
        !crn_s->hour.sched[hour])
        return 0;

    if (check_bound(MIN_MINUTE, MAX_MINUTE, min) &&
        !crn_s->minute.sched[min])
        return 0;

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

    if (check_bound(MIN_MONTH, MAX_MONTH, mon) &&
        !crn_s->month.sched[mon])
        exec = 0;

    if (check_bound(MIN_DAY_OF_WEEK, MAX_DAY_OF_WEEK, wday) &&
        !crn_s->day_of_week.sched[wday])
        exec = 0;

    if (check_bound(MIN_DAY_OF_MONTH, MAX_DAY_OF_MONTH, mday) &&
        !crn_s->day_of_month.sched[mday])
        exec = 0;
    
    if (check_bound(MIN_HOUR, MAX_HOUR, hour) &&
        !crn_s->hour.sched[hour])
        exec = 0;

    if (check_bound(MIN_MINUTE, MAX_MINUTE, min) &&
        !crn_s->minute.sched[min])
        exec = 0;

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
    } else if (**pos == '\'') {
        sep = '\'';
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
        if (sep == '\'' && !*end)
            return -1;

        siz = min(end - *pos, arg_size - 1);
        strncpy(arg, *pos, siz);
        arg[siz] = '\0';
        if (sep == '"')
            *pos = end + 1;
        else if (sep == '\'')
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

    pid = fork();
    if (pid == -1) {
        pr_err("Failed to fork\n");
        return;
    } else if (pid == 0) {
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

        pr_debug("Command: %s\n", comm_strip);
        pr_debug("Arguments: [");
        for (int i = 0; i < idx; ++i) {
            if (i < idx - 1)
                pr_debug("'%s', ", args[i]);
            else
                pr_debug("'%s'", args[i]);
        }
        pr_debug("]\n");
        execvp(comm_strip, args);
        perror("execvp");
out_free:
        for (int i = 0; i < MAX_ARG; ++i)
            free(args[i]);
        exit(-1);
    } else {
        waitpid(pid, NULL, 0);
    }
}

static int cron__sched(cron_set *crn_s, char *comm_args)
{
    if (crn_s == NULL)
        return -1;

    while (1) {
        if (cron__should_exec(crn_s))
            exec(comm_args);
        sleep(ONE_SEC);
    }
    return 0;
}

static char zero[] = "0";
static int is_legal(int tmp, char *pos, size_t n) {
    return !(tmp == 0 && strncmp(pos, zero, min(sizeof(zero), n)));
}

static void ses__write_sched(Ses *ses, const int _start, const int _end)
{
    const int start = max(0, _start);
    const int end = min(MAX_SCHED - 1, (_end == -1 ? MAX_SCHED - 1 : _end));

    for (int i = start; i <= end; i++)
        ses->sched[i] = 1;
}

static int parse_ses(char **pos, Ses *ses);

static int parse_step(char **pos, Ses *ses)
{
    int tmp;

    ++*pos;
    if (isdigit(**pos)) { // step
        char *end = NULL;
        for(end = *pos; isdigit(*end) && *end; ++end) {}
        tmp = atoin(*pos, end - *pos);
        if (!is_legal(tmp, *pos, end - *pos)) {
            pr_err("Can't be converted to integer\n");
            return -1;
        }
        *pos = end;
        ses->step = tmp;
    } else {
        pr_err("\'/\' should be followed with a number\n");
        return -1;
    }
    return 0;
}

static int parse_num_lhs(char **pos, Ses *ses)
{
    char *end = NULL;

    // consumes the number
    for(end = *pos; isdigit(*end) && *end; ++end) {}
    int tmp = atoin(*pos, end - *pos);
    if (!is_legal(tmp, *pos, end - *pos)) {
        pr_err("Can't be converted to integer\n");
        return -1;
    }
    *pos = end;
    ses->start = tmp;
    if (!**pos) {
        ses__write_sched(ses, ses->start, ses->start);
        return 0;
    } else if (**pos == '-') {
        ++*pos;
        if (isdigit(**pos)) {
            char *end = NULL;

            for(end = *pos; isdigit(*end) && *end; ++end) {}
            tmp = atoin(*pos, end - *pos);
            if (!is_legal(tmp, *pos, end - *pos)) {
                pr_err("Can't be converted to integer\n");
                return -1;
            }
            *pos = end;
            ses->end = tmp;
            if (**pos == '/') {
                ses__write_sched(ses, ses->start, ses->end);
                if (parse_step(pos, ses))
                    return -1;
            } else if (**pos == ',') {
                ses__write_sched(ses, ses->start, ses->end);
                ++*pos;
                parse_ses(pos, ses);
            } else if (!**pos) {
                // single number
                ses__write_sched(ses, ses->start, ses->end);
                return 0;
            } else {
                pr_err("Illegal character(%c)\n", **pos);
                pr_debug("il cha 1\n");
                return -1;
            }
        } else if (**pos == '*') {
            ++*pos;
            ses->end = -1;
            if (**pos == '/') {
                ses__write_sched(ses, ses->start, ses->end);
                if (parse_step(pos, ses))
                    return -1;
            } else if (!**pos) {
                // TODO: this is supposed to be legal, right?
                return -1;
            } else if (**pos == ',') {
                ses__write_sched(ses, ses->start, ses->end);
                ++*pos;
                parse_ses(pos, ses);
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
    } else if (**pos == ',') {
        ses__write_sched(ses, ses->start, ses->start);
        ++*pos;
        parse_ses(pos, ses);
    } else if (**pos == '/') {
        ses__write_sched(ses, ses->start, -1);
        if (parse_step(pos, ses))
            return -1;
    } else {
        pr_err("Illegal character(%c)\n", **pos);
        pr_debug("il cha 4\n");
        return -1;
    }
    return 0;
}

static int parse_asterisk_lhs(char **pos, Ses *ses)
{
    ++*pos;
    ses->start = -1;
    if (**pos == ',') {
        ses__write_sched(ses, ses->start, ses->start);
        ++*pos;
        parse_ses(pos, ses);
    } else if (**pos == '-') {
        ++*pos;
        if (isdigit(**pos)) {
            char *end = NULL;
            for(end = *pos; isdigit(*end) && *end; ++end) {}
            int tmp = atoin(*pos, end - *pos);
            if (!is_legal(tmp, *pos, end - *pos)) {
                pr_err("Can't be converted to integer\n");
                return -1;
            }
            *pos = end;
            ses->end = tmp;
            if (**pos == '/') {
                ses__write_sched(ses, ses->start, ses->start);
                if (parse_step(pos, ses))
                    return -1;
            } else if (**pos == ',') {
                ses__write_sched(ses, ses->start, ses->start);
                ++*pos;
                parse_ses(pos, ses);
            } else if (!**pos) {
                // range of numbers
                ses__write_sched(ses, ses->start, ses->start);
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
    } else if (!**pos) {
        // single asterisk, it's gonna be -1 -1
        ses__write_sched(ses, ses->start, ses->start);
        return 0;
    }  else if (**pos == '/') {
        ses__write_sched(ses, ses->start, ses->start);
        if (parse_step(pos, ses))
            return -1;
    } else {
        pr_err("Illegal character(%c)\n", **pos);
        pr_debug("il cha 6\n");
        return -1;
    }
    return 0;
}

/* caller will clear ses */
static int parse_ses(char **pos, Ses *ses)
{
    int err = 0;

    if (pos == NULL || ses == NULL)
        return -1;

    /*
    $root:
        (num | *)
        $num:
            (end | - | , | /)
            $-:
                (num | *)
                $num:
                    (/ | , | end)
                    $/:
                        (step)
                        $step:
                            (end)
                    $,:
                        (root)
            $,:
                (root)
            $/:  // this is broken and different from the definition
                (step)
                $step:
                    (end)
        $*:
            (, | - | end | /)
            $,:
                (root)
            $-:
                (num)
                $num:
                    (/ | , | end)
                    $/:
                        (step)
                        $step: end
                    $,:
                        (root)
            $/:
                (step)
                $step:
                    (end)
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

static void ses__get_ranges(const Ses *ses, char *ranges)
{
    const char *sched = ses->sched;
    int prev_start = -1;

    for (int i = 0; i < MAX_SCHED; i++) {
        if (sched[i]) {
            if (prev_start == -1)
                prev_start = i;
        } else if (!sched[i] && prev_start != -1) {
            ranges += sprintf(ranges, "%d-%d ", prev_start, i - 1);
            prev_start = -1;
        }
    }
}

static int parse(const char *vbuf, cron_set *crn_s, char *comm_args, size_t comm_args_len)
{
    /* get the raw pointer */
    char *pos = __vec__at(vbuf, 0);
    int len = vec__len(vbuf);
    char tok[NUM_LEN];
    int cnt = 0;
    size_t vbuf_siz;
    char ranges[256];
    static const char *time_types[] = { "minute", "hour", "day_of_month", "month", "day_of_week" };

    memset(tok, 0, sizeof(tok));

    for (int idx = 0;
         idx < CRON_NUM && get_next_tok(&pos, tok, sizeof(tok));
         ++idx, ++cnt, memset(tok, 0, sizeof(tok))) {
        int tmp;
        Ses *ses_tmp;
        char *pos = tok;

        memset(ranges, 0, sizeof(ranges));

        switch (idx) {
        case 0:
            ses_tmp = &crn_s->minute;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 1:
            ses_tmp = &crn_s->hour;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 2:
            ses_tmp = &crn_s->day_of_month;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 3:
            ses_tmp = &crn_s->month;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            break;
        case 4:
            ses_tmp = &crn_s->day_of_week;
            tmp = parse_ses(&pos, ses_tmp);
            if (tmp)
                return tmp;
            break;
        default:
            break;
        }
        ses__get_ranges(ses_tmp, ranges);
        ses_tmp->count = -1; /* dummy value to make step work */
        pr_debug("%-16s ranges: %s step: %d\n", time_types[idx],
                                                ranges[0] == 0 ? "All" : ranges,
                                                ses_tmp->step);
    }

    // TODO: replace with memchr
    // go to the first non-space
    vbuf_siz = vec__len(vbuf) * vec__mem_size(vbuf);
    for (;pos < (char *)__vec__at(vbuf, 0) + vbuf_siz &&
          *pos == ' '; ++pos) {}

    /* copy the rest of the string to comm_args */
    if (strncpy(comm_args, pos, min(vbuf_siz, comm_args_len)) == 0) {
        pr_err("Empty command\n");
        return -1;
    }

    if (cnt < CRON_NUM) {
        pr_err("Only has %d numbers, needs to be %d\n", cnt, CRON_NUM);
        return -1;
    }
    return 0;
}

static void print_help()
{
    printf(
        "\n  Cron by Howard Chu\n"
        "\n    -h: Print this message"
        "\n    -f <crontab file>: Path of the crontab file (default: ~/.crontab.txt)"
        "\n\n"
    );
}

static int start(int argc, char **argv)
{
    int err = 0;
    FILE *f;
    char *vbuf;
    cron_set crn_s;
    char comm_args[COMM_LEN];
    char cron_tab_file[PATH_MAX];
    int opt;
    int tmp;

    memset(cron_tab_file, 0, PATH_MAX);
    tmp = snprintf(cron_tab_file, sizeof(cron_tab_file) - 1, DEFAULT_CRONTAB_FMT, getenv(HOME));
    cron_tab_file[tmp] = 0;
    // parsing arguments to get the file name
    while ((opt = getopt(argc, argv, "hf:")) != -1) {
        int len;

        switch (opt) {
        case 'f':
            len = min(strlen(optarg), sizeof(cron_tab_file) - 1);
            strncpy(cron_tab_file, optarg, len);
            cron_tab_file[len] = 0;
            break;
        case 'h':
            print_help();
            return 0;
            break;
        default:
            printf("Option not found\n");
            print_help();
            return -1;
            break;
        }
    }
    pr_debug("Cron tab file location: %s\n", cron_tab_file);
    f = fopen(cron_tab_file, "r");
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

    memset(&crn_s, 0, sizeof(crn_s));
    memset(comm_args, 0, sizeof(comm_args));
    err = parse(vbuf, &crn_s, comm_args, sizeof(comm_args));
    cron__sched(&crn_s, comm_args);

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

int main(int argc, char **argv)
{
    return start(argc, argv);
}
