/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_TSI_CMDS_H_
#define __ASM_TSI_CMDS_H_

#include <linux/arm-smccc.h>
#include <asm/tsi_smc.h>
#include <asm/tsi_tmm.h>

static inline unsigned long tsi_get_version(void)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(SMC_TSI_ABI_VERSION, &res);

	return res.a0;
}

static inline unsigned long tsi_measurement_extend(struct cvm_measurement_extend *cvm_meas_ext)
{

	struct arm_smccc_res res;
	unsigned char value[MAX_MEASUREMENT_SIZE];

	memcpy(value, &cvm_meas_ext->value, sizeof(cvm_meas_ext->value));

	arm_smccc_1_1_smc(SMC_TSI_MEASUREMENT_EXTEND, cvm_meas_ext->index,
		cvm_meas_ext->size, virt_to_phys(value), &res);

	return res.a0;
}

static inline unsigned long tsi_measurement_read(struct cvm_measurement *cvm_meas)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(SMC_TSI_MEASUREMENT_READ, cvm_meas->index, &res);

	memcpy(cvm_meas->value, &res.a1, sizeof(cvm_meas->value));

	return res.a0;
}

static inline unsigned long tsi_attestation_token_init(struct cvm_attestation_cmd *attest_cmd)
{
	struct arm_smccc_res res;
	unsigned char challenge[CHALLENGE_SIZE];

	memcpy(challenge, attest_cmd->challenge, sizeof(attest_cmd->challenge));

	arm_smccc_1_1_smc(SMC_TSI_ATTESTATION_TOKEN_INIT, virt_to_phys(challenge), &res);

	return res.a0;
}

static inline unsigned long tsi_attestation_token_continue(struct cvm_attestation_cmd *attest_cmd)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(SMC_TSI_ATTESTATION_TOKEN_CONTINUE, virt_to_phys(attest_cmd->granule_ipa),
		attest_cmd->offset, attest_cmd->size, &res);

	attest_cmd->num_wr_bytes = res.a1;

	return res.a0;
}

static inline unsigned long tsi_get_device_cert(struct cca_device_cert *dev_cert,
	unsigned char *cert_buf)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(SMC_TSI_DEVICE_CERT, virt_to_phys(cert_buf), MAX_DEV_CERT_SIZE, &res);

	dev_cert->size = res.a1;

	return res.a0;
}

#endif  /* __ASM_TSI_CMDS_H_ */
