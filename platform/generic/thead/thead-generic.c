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
#include <sbi/sbi_heap.h>
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

static u64 errata;
struct thead_performance_status {
	bool valid;
	unsigned long smpen;
	unsigned long mccr2;
	unsigned long mxstatus;
	unsigned long mhint;
	unsigned long mhcr;
	unsigned long mhint2;
	unsigned long mhint4;
};

static unsigned long performance_status_ptr_offset;

extern struct sbi_platform platform;
static inline bool thead_has_hotplug(void)
{
	return platform.hart_count > 1;
}

static void hart_performance_save(void)
{
	struct sbi_scratch *scratch;
	struct thead_performance_status *status;
	scratch		 = sbi_scratch_thishart_ptr();
	status		 = sbi_scratch_read_type(scratch, void *,
						 performance_status_ptr_offset);
	status->smpen	 = csr_read(CSR_SMPEN);
	status->mccr2	 = csr_read(CSR_MCCR2);
	status->mxstatus = csr_read(CSR_MXSTATUS);
	status->mhint	 = csr_read(CSR_MHINT);
	status->mhcr	 = csr_read(CSR_MHCR);
	status->mhint2	 = csr_read(CSR_MHINT2_E);
	status->mhint4	 = csr_read(CSR_MHINT4);
	status->valid	 = true;
}

static void hart_performance_restore(struct thead_performance_status *status)
{
	if (!status->valid)
		return;

	csr_write(CSR_SMPEN, status->smpen);
	csr_write(CSR_MCCR2, status->mccr2);
	csr_write(CSR_MXSTATUS, status->mxstatus);
	csr_write(CSR_MHINT, status->mhint);
	csr_write(CSR_MHCR, status->mhcr);
	csr_write(CSR_MHINT2_E, status->mhint2);
	csr_write(CSR_MHINT4, status->mhint4);
	status->valid = false;
}

struct thead_performance_status* hart_performance_init(bool cold_boot)
{
	struct thead_performance_status *status;
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	if (cold_boot)
		performance_status_ptr_offset = sbi_scratch_alloc_type_offset(
			struct thead_performance_status *);
	if (!performance_status_ptr_offset)
		return NULL;

	status = sbi_scratch_read_type(scratch, void *,
				       performance_status_ptr_offset);
	if (!status) {
		status = sbi_zalloc(sizeof(*status));
		if (!status)
			return NULL;
		sbi_scratch_write_type(scratch, void *,
				       performance_status_ptr_offset, status);
	}
	return status;
}

static int thead_early_init(bool cold_boot)
{
	struct thead_performance_status *status;

	if (thead_has_hotplug()) {
		status = hart_performance_init(cold_boot);
		if (!status)
			return SBI_ENOMEM;
		hart_performance_restore(status);
	}

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

static void thead_final_exit(void)
{
	if (thead_has_hotplug())
		hart_performance_save();
}

static int thead_generic_platform_init(const void *fdt, int nodeoff,
				       const struct fdt_match *match)
{
	const struct thead_generic_quirks *quirks = match->data;
	errata = quirks->errata;

	generic_platform_ops.early_init = thead_early_init;
	generic_platform_ops.extensions_init = thead_extensions_init;
	generic_platform_ops.final_exit = thead_final_exit;

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
