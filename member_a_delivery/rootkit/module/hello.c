#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

static int
hello_modevent(module_t module, int event, void *arg)
{
    (void)module;
    (void)arg;

    switch (event) {
    case MOD_LOAD:
        printf("rootkit-lab: benign baseline module loaded\n");
        break;
    case MOD_UNLOAD:
        printf("rootkit-lab: benign baseline module unloaded\n");
        break;
    default:
        break;
    }
    return (0);
}

static moduledata_t hello_mod = {
    "rootkit_lab_baseline",
    hello_modevent,
    NULL
};

MODULE_VERSION(rootkit_lab_baseline, 1);
DECLARE_MODULE(rootkit_lab_baseline, hello_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
