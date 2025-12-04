/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/fdt/fdt_helper.h>

struct thead_generic_quirks {
	u64	errata;
};

static u64 errata;

static int thead_early_init(bool cold_boot)
{
	if (errata & THEAD_QUIRK_ERRATA_TLB_FLUSH)
		thead_register_tlb_flush_trap_handler();

	return generic_early_init(cold_boot);
}

static int thead_extensions_init(struct sbi_hart_features *hfeatures)
{
	int rc;

	rc = generic_extensions_init(hfeatures);
	if (rc)
		return rc;

	if (errata & THEAD_QUIRK_ERRATA_THEAD_PMU)
		thead_c9xx_register_pmu_device();

	return 0;
}

static int thead_generic_platform_init(const void *fdt, int nodeoff,
				       const struct fdt_match *match)
{
	const struct thead_generic_quirks *quirks = match->data;
	errata = quirks->errata;

	generic_platform_ops.early_init = thead_early_init;
	generic_platform_ops.extensions_init = thead_extensions_init;

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
