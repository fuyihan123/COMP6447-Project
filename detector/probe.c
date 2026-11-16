/* probe - kernel inspection and behaviour probes for detect. */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <elf.h>

extern char **environ;

#define KMEM_PATH	"/dev/kmem"
#define KERNEL_PATH	"/boot/kernel/kernel"
#define PROBE_UID	1001
#define PROLOGUE_LEN	16

static int
kmem_read(uint64_t addr, void *buf, size_t len)
{
	static int fd = -1;
	ssize_t r;

	if (fd < 0)
		fd = open(KMEM_PATH, O_RDONLY);
	if (fd < 0)
		return (-1);
	r = pread(fd, buf, len, (off_t)addr);
	return (r == (ssize_t)len ? 0 : -1);
}

/* Runtime VA -> file offset via ELF program headers (KASLR safe). */
static int
elf_file_offset(uint64_t vaddr, off_t *out)
{
	Elf64_Ehdr eh;
	Elf64_Phdr ph;
	int fd, i;

	fd = open(KERNEL_PATH, O_RDONLY);
	if (fd < 0)
		return (-1);
	if (pread(fd, &eh, sizeof(eh), 0) != (ssize_t)sizeof(eh) ||
	    memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0) {
		close(fd);
		return (-1);
	}
	for (i = 0; i < eh.e_phnum; i++) {
		if (pread(fd, &ph, sizeof(ph),
		    (off_t)(eh.e_phoff + (uint64_t)i * eh.e_phentsize)) !=
		    (ssize_t)sizeof(ph))
			break;
		if (ph.p_type != PT_LOAD)
			continue;
		if (vaddr >= ph.p_vaddr && vaddr < ph.p_vaddr + ph.p_memsz) {
			*out = (off_t)(ph.p_offset + (vaddr - ph.p_vaddr));
			close(fd);
			return (0);
		}
	}
	close(fd);
	return (-1);
}

static int
kernel_range(uint64_t *basep, uint64_t *sizep)
{
	char line[256], name[64], addr[64], size[64];
	FILE *f;

	f = popen("kldstat", "r");
	if (f == NULL)
		return (-1);
	while (fgets(line, sizeof(line), f) != NULL) {
		if (sscanf(line, "%*d %*d %63s %63s %63s", addr, size, name) != 3)
			continue;
		if (strcmp(name, "kernel") != 0)
			continue;
		*basep = strtoull(addr, NULL, 0);
		*sizep = strtoull(size, NULL, 16);
		pclose(f);
		return (0);
	}
	pclose(f);
	return (-1);
}

static int
load_syms(const char *const *names, int n, uint64_t *out)
{
	char line[256], addr[64], name[192];
	FILE *f;
	int i;

	for (i = 0; i < n; i++)
		out[i] = 0;
	f = popen("nm " KERNEL_PATH " 2>/dev/null", "r");
	if (f == NULL)
		return (-1);
	while (fgets(line, sizeof(line), f) != NULL) {
		char type;
		if (sscanf(line, "%63s %c %191s", addr, &type, name) != 3)
			continue;
		for (i = 0; i < n; i++) {
			if (out[i] == 0 && strcmp(name, names[i]) == 0)
				out[i] = strtoull(addr, NULL, 16);
		}
	}
	pclose(f);
	return (0);
}

static int
mode_sysent(void)
{
	struct sysent *tab;
	uint64_t base, size, addr;
	size_t total;
	int i, rc = 0;

	if (kernel_range(&base, &size) != 0) {
		fprintf(stderr, "probe: cannot determine kernel range\n");
		return (2);
	}
	if (load_syms((const char *const[]){"sysent"}, 1, &addr) != 0 || addr == 0) {
		fprintf(stderr, "probe: cannot resolve sysent\n");
		return (2);
	}
	total = (size_t)SYS_MAXSYSCALL * sizeof(struct sysent);
	tab = malloc(total);
	if (tab == NULL)
		return (2);
	if (kmem_read(addr, tab, total) != 0) {
		fprintf(stderr, "probe: cannot read %s (root required)\n", KMEM_PATH);
		free(tab);
		return (2);
	}
	for (i = 0; i < SYS_MAXSYSCALL; i++) {
		uint64_t call = (uint64_t)(uintptr_t)tab[i].sy_call;
		if (call < base || call >= base + size) {
			printf("SYENT %d 0x%llx\n", i, (unsigned long long)call);
			rc = 1;
		}
	}
	free(tab);
	return (rc);
}

static const char *const prologue_syms[] = {
	"sys_kill", "sys_getdirentries", "sys_kldnext", "sys_kldstat",
	"sys_kldfind", "sys_kldfirstmod", "sys_kldsym", "kern_fcntl",
	"ttydisc_read", "sys_rmdir", "sys_execve", "sys_setuid",
	"sys_openat", "sys_fstatat", "sys_access", "sys_unlinkat",
	"sys_readlinkat", "sys_read", "sys_pread", "sys___sysctl",
	"sys_readv", "sys_preadv", "sys_chmod", "sys_fchmodat",
};

static int
mode_prologue(void)
{
	uint64_t base, size, addrs[nitems(prologue_syms)];
	unsigned char mem[PROLOGUE_LEN], file[PROLOGUE_LEN];
	int i, rc = 0;
	int kfd;

	if (kernel_range(&base, &size) != 0) {
		fprintf(stderr, "probe: cannot determine kernel range\n");
		return (2);
	}
	if (load_syms(prologue_syms, nitems(prologue_syms), addrs) != 0)
		return (2);
	kfd = open(KERNEL_PATH, O_RDONLY);
	if (kfd < 0) {
		fprintf(stderr, "probe: cannot open %s\n", KERNEL_PATH);
		return (2);
	}
	for (i = 0; i < (int)nitems(prologue_syms); i++) {
		off_t off;

		if (addrs[i] == 0)
			continue;
		if (elf_file_offset(addrs[i], &off) != 0)
			continue;
		if (kmem_read(addrs[i], mem, sizeof(mem)) != 0)
			continue;
		if (pread(kfd, file, sizeof(file), off) != (ssize_t)sizeof(file))
			continue;
		if (memcmp(mem, file, sizeof(mem)) != 0) {
			printf("PROLOGUE %s\n", prologue_syms[i]);
			rc = 1;
		}
	}
	close(kfd);
	return (rc);
}

static int
mode_kldcheck(int argc, char **argv)
{
	int i, rc = 0, fileid;

	for (i = 0; i < argc; i++) {
		struct kld_file_stat st;
		int id;

		id = kldfind(argv[i]);
		if (id < 0)
			continue;
		st.version = sizeof(st);
		if (kldstat(id, &st) != 0) {
			printf("HIDDEN %s id=%d\n", argv[i], id);
			rc = 1;
		}
	}
	/* A listed file that kldfind cannot resolve is equally inconsistent. */
	for (fileid = kldnext(0); fileid > 0; fileid = kldnext(fileid)) {
		struct kld_file_stat st;

		st.version = sizeof(st);
		if (kldstat(fileid, &st) != 0)
			continue;
		if (kldfind(st.name) < 0) {
			printf("INCONSISTENT %s id=%d\n", st.name, fileid);
			rc = 1;
		}
	}
	return (rc);
}

/* Run a trigger as uid 1001 and report the resulting uid. */

static void
drop_privs(void)
{
	(void)setgid(PROBE_UID);
	(void)setuid(PROBE_UID);
}

/* Block SIGSYS from unimplemented syscall numbers. */
static void
block_sigsys(void)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGSYS);
	(void)sigprocmask(SIG_BLOCK, &set, NULL);
}

typedef void (*actfn_t)(int argc, char **argv);

static void
run_child(const char *tag, actfn_t fn, int argc, char **argv)
{
	int p[2];
	pid_t pid;
	char buf[1024];
	ssize_t n;
	int st;

	if (pipe(p) != 0) {
		perror("pipe");
		return;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(p[0]);
		close(p[1]);
		return;
	}
	if (pid == 0) {
		close(p[0]);
		alarm(6);
		if (dup2(p[1], STDOUT_FILENO) < 0)
			_exit(127);
		close(p[1]);
		drop_privs();
		block_sigsys();
		fn(argc, argv);
		printf("RESULT %s uid=%u euid=%u\n", tag,
		    (unsigned)getuid(), (unsigned)geteuid());
		fflush(stdout);
		_exit(0);
	}
	close(p[1]);
	n = read(p[0], buf, sizeof(buf) - 1);
	close(p[0]);
	waitpid(pid, &st, 0);
	if (n > 0) {
		buf[n] = '\0';
		fputs(buf, stdout);
	} else {
		printf("RESULT %s noreport\n", tag);
	}
}

static void
a_kill_sig(int argc, char **argv)
{
	long sig = argc > 0 ? strtol(argv[0], NULL, 0) : 31;

	if (sig > 0 && sig < NSIG)
		(void)signal((int)sig, SIG_IGN);
	(void)kill(getpid(), (int)sig);
}

static void
a_syscall_num(int argc, char **argv)
{
	int n = argc > 0 ? (int)strtol(argv[0], NULL, 0) : 210;

	(void)syscall(n);
}

static void
a_syscall_str(int argc, char **argv)
{
	int n = argc > 0 ? (int)strtol(argv[0], NULL, 0) : 210;

	(void)syscall(n, argc > 1 ? argv[1] : "");
}

static void
a_sysctl6969(int argc __unused, char **argv __unused)
{
	int v = 6969;

	(void)sysctlbyname("kern.sched.preempt_threshold", NULL, NULL,
	    &v, sizeof(v));
}

static void
a_write_dev(int argc, char **argv)
{
	int fd;

	if (argc < 2)
		return;
	fd = open(argv[0], O_WRONLY);
	if (fd >= 0) {
		(void)write(fd, argv[1], strlen(argv[1]));
		close(fd);
	}
}

static void
a_ioctl_rk(int argc __unused, char **argv __unused)
{
	unsigned long magic = (unsigned long)0x524b454c45564154ULL;
	int fd;

	fd = open("/dev/rk", O_RDWR);
	if (fd >= 0) {
		(void)ioctl(fd, magic, 0);
		(void)ioctl(fd, 0, magic);
		close(fd);
	}
}

static void
a_read_magic(int argc __unused, char **argv __unused)
{
	(void)read(0xba115, NULL, 0xd1e);
}

static void
a_exit_magic(int argc __unused, char **argv __unused)
{
	(void)syscall(SYS_exit, 0xBEABAD00);
}

static void
a_setuid0(int argc __unused, char **argv __unused)
{
	(void)setuid(0);
}

static void
a_stat_path(int argc, char **argv)
{
	struct stat st;

	if (argc > 0)
		(void)stat(argv[0], &st);
}

static void
a_modfind_syscall(int argc, char **argv)
{
	struct module_stat ms;
	int mid;

	if (argc < 1)
		return;
	mid = modfind(argv[0]);
	if (mid < 0)
		return;
	ms.version = sizeof(ms);
	if (modstat(mid, &ms) != 0)
		return;
	(void)syscall(ms.data.intval);
}

static void
beh_exec_pwnme(void)
{
	char path[64], cmd[160];
	char *args[4];
	pid_t pid;
	int st;
	FILE *f;

	snprintf(path, sizeof(path), "/tmp/.rkprobe.%d", (int)getpid());
	(void)unlink(path);
	pid = fork();
	if (pid == 0) {
		drop_privs();
		snprintf(cmd, sizeof(cmd), "id -u > %s; exit 0", path);
		args[0] = "PWNME";
		args[1] = "-c";
		args[2] = cmd;
		args[3] = NULL;
		execve("/bin/sh", args, environ);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	f = fopen(path, "r");
	if (f != NULL) {
		char line[32];

		if (fgets(line, sizeof(line), f) != NULL) {
			line[strcspn(line, "\n")] = '\0';
			printf("RESULT exec_pwnme uid=%s\n", line);
		}
		fclose(f);
	} else {
		printf("RESULT exec_pwnme noreport\n");
	}
	(void)unlink(path);
}

static int
mode_behavior(int argc, char **argv)
{
	const char *what;

	if (argc < 1) {
		fprintf(stderr, "usage: probe behavior <case> [args]\n");
		return (2);
	}
	what = argv[0];
	argc--;
	argv++;

	if (strcmp(what, "kill_sig") == 0)
		run_child("kill_sig", a_kill_sig, argc, argv);
	else if (strcmp(what, "syscall_num") == 0)
		run_child("syscall_num", a_syscall_num, argc, argv);
	else if (strcmp(what, "syscall_str") == 0)
		run_child("syscall_str", a_syscall_str, argc, argv);
	else if (strcmp(what, "sysctl6969") == 0)
		run_child("sysctl6969", a_sysctl6969, 0, NULL);
	else if (strcmp(what, "write_dev") == 0)
		run_child("write_dev", a_write_dev, argc, argv);
	else if (strcmp(what, "ioctl_rk") == 0)
		run_child("ioctl_rk", a_ioctl_rk, 0, NULL);
	else if (strcmp(what, "read_magic") == 0)
		run_child("read_magic", a_read_magic, 0, NULL);
	else if (strcmp(what, "exit_magic") == 0)
		run_child("exit_magic", a_exit_magic, 0, NULL);
	else if (strcmp(what, "setuid0") == 0)
		run_child("setuid0", a_setuid0, 0, NULL);
	else if (strcmp(what, "stat_path") == 0)
		run_child("stat_path", a_stat_path, argc, argv);
	else if (strcmp(what, "modfind_syscall") == 0)
		run_child("modfind_syscall", a_modfind_syscall, argc, argv);
	else if (strcmp(what, "exec_pwnme") == 0)
		beh_exec_pwnme();
	else {
		fprintf(stderr, "probe: unknown behaviour %s\n", what);
		return (2);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: probe sysent|prologue|kldcheck|behavior ...\n");
		return (2);
	}
	if (strcmp(argv[1], "sysent") == 0)
		return (mode_sysent());
	if (strcmp(argv[1], "prologue") == 0)
		return (mode_prologue());
	if (strcmp(argv[1], "kldcheck") == 0)
		return (mode_kldcheck(argc - 2, argv + 2));
	if (strcmp(argv[1], "behavior") == 0)
		return (mode_behavior(argc - 2, argv + 2));
	fprintf(stderr, "probe: unknown mode %s\n", argv[1]);
	return (2);
}
