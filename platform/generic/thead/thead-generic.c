/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <thead/c9xx_encoding.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/fdt/fdt_helper.h>

/* redefine CSR register */
#define CSR_MXSTATUS	THEAD_C9XX_CSR_MXSTATUS
#define CSR_MHCR	THEAD_C9XX_CSR_MHCR
#define CSR_MCCR2	THEAD_C9XX_CSR_MCCR2
#define CSR_MHINT	THEAD_C9XX_CSR_MHINT
#define CSR_MHINT2_E	THEAD_C9XX_CSR_MHINT2
#define CSR_MHINT4	THEAD_C9XX_CSR_MHINT4
#define CSR_MSMPR	THEAD_C9XX_CSR_MSMPR
#define CSR_SMPEN	CSR_MSMPR

struct thead_generic_quirks {
	u64	errata;
};

extern struct sbi_platform platform;

static inline bool thead_has_hotplug(void)
{
	return platform.hart_count > 1;
}

static void thead_suspend_non_ret_save(unsigned long *data)
{
	data[0] = csr_read(CSR_SMPEN);
	data[1] = csr_read(CSR_MCCR2);
	data[2] = csr_read(CSR_MXSTATUS);
	data[3] = csr_read(CSR_MHINT);
	data[4] = csr_read(CSR_MHCR);
	data[5] = csr_read(CSR_MHINT2_E);
	data[6] = csr_read(CSR_MHINT4);
}

static void thead_suspend_non_ret_restore(unsigned long *data)
{
	csr_write(CSR_SMPEN, data[0]);
	csr_write(CSR_MCCR2, data[1]);
	csr_write(CSR_MXSTATUS, data[2]);
	csr_write(CSR_MHINT, data[3]);
	csr_write(CSR_MHCR, data[4]);
	csr_write(CSR_MHINT2_E, data[5]);
	csr_write(CSR_MHINT4, data[6]);
}

static int thead_nascent_init(void)
{

	if (thead_has_hotplug()) {
		platform.suspend_backup_csr_count = 7;
		generic_platform_ops.suspend_non_ret_save = thead_suspend_non_ret_save;
		generic_platform_ops.suspend_non_ret_restore = thead_suspend_non_ret_restore;
	}

	return generic_nascent_init();
}

static int thead_tlb_flush_early_init(bool cold_boot)
{
	thead_register_tlb_flush_trap_handler();

	return generic_early_init(cold_boot);
}

static int thead_pmu_extensions_init(struct sbi_hart_features *hfeatures)
{
	int rc;

	rc = generic_extensions_init(hfeatures);
	if (rc)
		return rc;

	thead_c9xx_register_pmu_device();

	return 0;
}

static int thead_generic_platform_init(const void *fdt, int nodeoff,
				       const struct fdt_match *match)
{
	const struct thead_generic_quirks *quirks = match->data;

	generic_platform_ops.nascent_init = thead_nascent_init;
	if (quirks->errata & THEAD_QUIRK_ERRATA_TLB_FLUSH)
		generic_platform_ops.early_init = thead_tlb_flush_early_init;
	if (quirks->errata & THEAD_QUIRK_ERRATA_THEAD_PMU)
		generic_platform_ops.extensions_init = thead_pmu_extensions_init;

	return 0;
}

static const struct thead_generic_quirks thead_th1520_quirks = {
	.errata = THEAD_QUIRK_ERRATA_TLB_FLUSH | THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static const struct thead_generic_quirks thead_pmu_quirks = {
	.errata = THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static const struct fdt_match thead_generic_match[] = {
	{ .compatible = "canaan,kendryte-k230", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,cv1800b", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,cv1812h", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2000", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2002", .data = &thead_pmu_quirks },
	{ .compatible = "sophgo,sg2044", .data = &thead_pmu_quirks },
	{ .compatible = "thead,th1520", .data = &thead_th1520_quirks },
	{ },
};

const struct fdt_driver thead_generic = {
	.match_table		= thead_generic_match,
	.init			= thead_generic_platform_init,
};
