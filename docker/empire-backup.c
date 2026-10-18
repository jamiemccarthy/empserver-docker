/*
 * empire-backup: pre-update backup for the Empire server container.
 *
 * emp_server runs the econfig `pre_update_hook` before every game update,
 * holding its update lock so nothing writes the data directory meanwhile. It
 * does so with glibc's system(), which always executes "/bin/sh -c <command>".
 * The image has no shell, so this one static binary plays two roles:
 *
 *   - Installed as /usr/local/sbin/empire-backup, it takes the backup.
 *   - /bin/sh is a symlink to it. When invoked under the name "sh" it accepts
 *     exactly   sh -c -- /usr/local/sbin/empire-backup   (what glibc's
 *     system() runs, sysdeps/posix/system.c) or the same without "--", and
 *     takes the backup itself; any other invocation prints an error and
 *     exits 127.
 *
 * The backup, in order:
 *   1. Write DATA_DIR as a gzip-compressed ustar tar to BACKUP_DIR/<stamp>.tar.gz
 *      (via a hidden .tmp file and rename(), so readers never see a partial
 *      archive). The top-level server.log is left out.
 *   2. Delete all but the newest `keep` archives (EMPIRE_BACKUP_KEEP, default 288).
 *   3. If server.log exceeds 4 MiB, cut it down to its last 1 MiB in place.
 *
 * Backup mode always exits 0: a non-zero exit makes Empire skip the update.
 *
 * Build: gcc -O2 -Wall -Wextra -static -o empire-backup empire-backup.c -lz
 * DATA_DIR and BACKUP_DIR can be overridden at compile time (-DDATA_DIR=...).
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#ifndef DATA_DIR
#define DATA_DIR "/usr/local/var/empire"
#endif
#ifndef BACKUP_DIR
#define BACKUP_DIR "/backup"
#endif

#define SELF_PATH "/usr/local/sbin/empire-backup"
#define DEFAULT_KEEP 288
#define LOG_NAME "server.log"
#define LOG_LIMIT 4194304L
#define LOG_KEEP 1048576L
#define BLOCK 512

static gzFile out;		/* archive being written */
static int failed;		/* set on the first error */
static char reason[512];	/* what went wrong, for the failure line */

static void
fail(const char *what, const char *path)
{
	if (failed)
		return;
	failed = 1;
	snprintf(reason, sizeof reason, "%s%s%.300s: %s", what,
		 path ? " " : "", path ? path : "", strerror(errno));
}

/* ---- tar writing ------------------------------------------------------ */

static void
put_bytes(const void *buf, size_t len)
{
	if (failed || len == 0)
		return;
	if (gzwrite(out, buf, (unsigned)len) != (int)len)
		fail("write to archive", NULL);
}

static void
put_zeros(size_t len)
{
	static const char zeros[BLOCK];

	while (len > 0) {
		size_t n = len < sizeof zeros ? len : sizeof zeros;
		put_bytes(zeros, n);
		len -= n;
	}
}

/* Store `value` as zero-padded octal in a field of `width` bytes (NUL last). */
static int
put_octal(char *field, size_t width, unsigned long long value)
{
	char tmp[32];

	snprintf(tmp, sizeof tmp, "%0*llo", (int)(width - 1), value);
	if (strlen(tmp) != width - 1)
		return -1;	/* too big for the field */
	memcpy(field, tmp, width);
	return 0;
}

/*
 * Write one ustar header. `name` is the archive path (directories already end
 * in '/'). Returns 0, or -1 with the entry skipped and a note on stderr.
 */
static int
put_header(const char *name, const struct stat *st, char type, off_t size)
{
	unsigned char h[BLOCK];
	size_t len = strlen(name);
	const char *prefix = NULL;
	size_t prefix_len = 0, i;
	unsigned sum = 0;
	char tmp[8];

	memset(h, 0, sizeof h);

	if (len > 100) {
		/* split at a '/' so the prefix is <= 155 and the rest <= 100 */
		for (i = len; i-- > 0;) {
			if (name[i] == '/' && i <= 155 && len - i - 1 <= 100 &&
			    len - i - 1 > 0) {
				prefix = name;
				prefix_len = i;
				break;
			}
		}
		if (!prefix) {
			fprintf(stderr, "empire-backup: skipping %s: name too long\n", name);
			return -1;
		}
		memcpy(h + 345, prefix, prefix_len);
		memcpy(h, name + prefix_len + 1, len - prefix_len - 1);
	} else {
		memcpy(h, name, len);
	}

	if (put_octal((char *)h + 100, 8, st->st_mode & 07777) ||
	    put_octal((char *)h + 108, 8, st->st_uid) ||
	    put_octal((char *)h + 116, 8, st->st_gid) ||
	    put_octal((char *)h + 124, 12, (unsigned long long)size) ||
	    put_octal((char *)h + 136, 12,
		      st->st_mtime < 0 ? 0 : (unsigned long long)st->st_mtime)) {
		fprintf(stderr, "empire-backup: skipping %s: value too large for tar\n", name);
		return -1;
	}
	h[156] = (unsigned char)type;
	memcpy(h + 257, "ustar\0" "00", 8);	/* magic + version */

	/* checksum is computed with the checksum field as eight spaces */
	memset(h + 148, ' ', 8);
	for (i = 0; i < BLOCK; i++)
		sum += h[i];
	snprintf(tmp, sizeof tmp, "%06o", sum);
	memcpy(h + 148, tmp, 6);
	h[154] = 0;
	h[155] = ' ';

	put_bytes(h, BLOCK);
	return 0;
}

static void
add_file(const char *path, const struct stat *st)
{
	FILE *f;
	char buf[65536];
	off_t left = st->st_size;

	/* open first so an unreadable file is skipped before its header is written */
	f = fopen(path, "rb");
	if (!f) {
		fail("open", path);
		return;
	}
	if (put_header(path, st, '0', st->st_size) < 0) {
		fclose(f);
		return;
	}
	while (left > 0 && !failed) {
		size_t want = left < (off_t)sizeof buf ? (size_t)left : sizeof buf;
		size_t got = fread(buf, 1, want, f);

		if (got == 0) {
			/* file shrank: pad with zeros so the archive stays well formed */
			fail("read (file shrank)", path);
			put_zeros((size_t)left);
			break;
		}
		put_bytes(buf, got);
		left -= (off_t)got;
	}
	fclose(f);
	if (st->st_size % BLOCK)
		put_zeros(BLOCK - (size_t)(st->st_size % BLOCK));
}

/* Add the directory `path` (e.g. "." or "./sub") and everything below it. */
static void
add_tree(const char *path)
{
	DIR *d;
	struct dirent *e;
	struct stat st;
	char child[PATH_MAX], dirname[PATH_MAX];

	if (lstat(path, &st) < 0) {
		fail("stat", path);
		return;
	}
	snprintf(dirname, sizeof dirname, "%s/", path);
	if (put_header(dirname, &st, '5', 0) < 0)
		return;

	d = opendir(path);
	if (!d) {
		fail("opendir", path);
		return;
	}
	while (!failed && (e = readdir(d)) != NULL) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		if (snprintf(child, sizeof child, "%s/%s", path, e->d_name) >=
		    (int)sizeof child) {
			fprintf(stderr, "empire-backup: skipping %s/%s: path too long\n",
				path, e->d_name);
			continue;
		}
		if (!strcmp(child, "./" LOG_NAME))
			continue;
		if (lstat(child, &st) < 0) {
			fail("stat", child);
			break;
		}
		if (S_ISDIR(st.st_mode))
			add_tree(child);
		else if (S_ISREG(st.st_mode))
			add_file(child, &st);
		else
			fprintf(stderr, "empire-backup: skipping %s: not a regular file or directory\n",
				child);
	}
	closedir(d);
}

/* ---- pruning and trimming --------------------------------------------- */

static int
ends_with(const char *s, const char *suffix)
{
	size_t a = strlen(s), b = strlen(suffix);

	return a >= b && !strcmp(s + a - b, suffix);
}

static int
compare_desc(const void *a, const void *b)
{
	return strcmp(*(char *const *)b, *(char *const *)a);
}

static long
keep_count(void)
{
	const char *s = getenv("EMPIRE_BACKUP_KEEP");
	char *end;
	long n;

	if (!s || !*s)
		return DEFAULT_KEEP;
	errno = 0;
	n = strtol(s, &end, 10);
	if (errno || *end || n < 1)
		return DEFAULT_KEEP;
	return n;
}

/* Delete all but the newest `keep` *.tar.gz archives; stamps sort by time. */
static void
prune(long keep)
{
	DIR *d = opendir(BACKUP_DIR);
	struct dirent *e;
	char **names = NULL;
	size_t count = 0, cap = 0, i;

	if (!d)
		return;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.' || !ends_with(e->d_name, ".tar.gz"))
			continue;
		if (count == cap) {
			char **grown = realloc(names, (cap ? cap * 2 : 64) * sizeof *names);

			if (!grown)
				break;
			names = grown;
			cap = cap ? cap * 2 : 64;
		}
		names[count] = strdup(e->d_name);
		if (!names[count])
			break;
		count++;
	}
	closedir(d);

	qsort(names, count, sizeof *names, compare_desc);
	for (i = 0; i < count; i++) {
		if (i >= (size_t)keep) {
			char path[PATH_MAX];

			snprintf(path, sizeof path, "%s/%s", BACKUP_DIR, names[i]);
			unlink(path);
		}
		free(names[i]);
	}
	free(names);
}

/*
 * Cut server.log to its last LOG_KEEP bytes, keeping the same inode:
 * emp_server holds it open with O_APPEND, so its next line lands at the new end.
 */
static void
trim_log(void)
{
	struct stat st;
	char *buf;
	int fd;
	ssize_t got;

	if (stat(LOG_NAME, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size <= LOG_LIMIT)
		return;
	buf = malloc(LOG_KEEP);
	fd = open(LOG_NAME, O_RDWR);
	if (!buf || fd < 0) {
		fprintf(stderr, "empire-backup: cannot trim %s: %s\n", LOG_NAME, strerror(errno));
		goto done;
	}
	got = pread(fd, buf, LOG_KEEP, st.st_size - LOG_KEEP);
	if (got != LOG_KEEP) {
		fprintf(stderr, "empire-backup: cannot read %s\n", LOG_NAME);
		goto done;
	}
	if (ftruncate(fd, 0) < 0 || pwrite(fd, buf, LOG_KEEP, 0) != LOG_KEEP)
		fprintf(stderr, "empire-backup: trimming %s failed: %s\n", LOG_NAME,
			strerror(errno));
done:
	if (fd >= 0)
		close(fd);
	free(buf);
}

/* ---- the backup ------------------------------------------------------- */

static void
take_backup(void)
{
	char stamp[32], tmp[PATH_MAX], final[PATH_MAX];
	time_t now = time(NULL);
	struct tm tm;

	gmtime_r(&now, &tm);
	strftime(stamp, sizeof stamp, "%Y%m%dT%H%M%SZ", &tm);
	snprintf(tmp, sizeof tmp, "%s/.%s.tar.gz.tmp", BACKUP_DIR, stamp);
	snprintf(final, sizeof final, "%s/%s.tar.gz", BACKUP_DIR, stamp);

	if (chdir(DATA_DIR) < 0)
		fail("chdir", DATA_DIR);
	else if (mkdir(BACKUP_DIR, 0777) < 0 && errno != EEXIST)
		fail("mkdir", BACKUP_DIR);
	else if (!(out = gzopen(tmp, "wb6")))
		fail("create", tmp);
	else {
		add_tree(".");
		put_zeros(2 * BLOCK);	/* end-of-archive marker */
		if (gzclose(out) != Z_OK)
			fail("finish archive", tmp);
		out = NULL;
		if (!failed && rename(tmp, final) < 0)
			fail("rename", final);
		if (failed)
			unlink(tmp);
	}

	if (failed) {
		fprintf(stderr, "empire-backup: %s\n", reason);
		fprintf(stderr, "empire-backup failed %s\n", stamp);
	} else {
		printf("empire-backup ok %s\n", stamp);
	}
	fflush(stdout);
	fflush(stderr);

	prune(keep_count());
	if (chdir(DATA_DIR) == 0)
		trim_log();
}

/* Whether argv is "sh -c [--] SELF_PATH", the only command the stand-in runs. */
static int
is_backup_command(int argc, char **argv)
{
	if (argc == 3)
		return !strcmp(argv[1], "-c") && !strcmp(argv[2], SELF_PATH);
	if (argc == 4)
		return !strcmp(argv[1], "-c") && !strcmp(argv[2], "--") &&
		    !strcmp(argv[3], SELF_PATH);
	return 0;
}

int
main(int argc, char **argv)
{
	const char *base = strrchr(argv[0], '/');

	base = base ? base + 1 : argv[0];
	if (!strcmp(base, "sh") && !is_backup_command(argc, argv)) {
		fputs("sh: this image has no shell; only \"-c [--] " SELF_PATH "\" is accepted\n",
		      stderr);
		return 127;
	}
	take_backup();
	return 0;
}
