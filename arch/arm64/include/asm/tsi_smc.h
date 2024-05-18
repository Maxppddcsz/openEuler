/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_TSI_SMC_H_
#define __ASM_TSI_SMC_H_

#define SMC_TSI_CALL_BASE           0xC4000000
#define TSI_ABI_VERSION_MAJOR       1
#define TSI_ABI_VERSION_MINOR       0
#define TSI_ABI_VERSION             ((TSI_ABI_VERSION_MAJOR << 16) | \
									TSI_ABI_VERSION_MINOR)

#define TSI_ABI_VERSION_GET_MAJOR(_version) ((_version) >> 16)
#define TSI_ABI_VERSION_GET_MINOR(_version) ((_version) & 0xFFFF)

#define TSI_SUCCESS             0
#define TSI_ERROR_INPUT         1
#define TSI_ERROR_STATE         2
#define TSI_INCOMPLETE          3

#define SMC_TSI_FID(_x)                        (SMC_TSI_CALL_BASE + (_x))
#define SMC_TSI_ABI_VERSION                    SMC_TSI_FID(0x190)

/*
 * arg1 == Index, which measurements slot to read
 * ret0 == Status / error
 * ret1 == Measurement value, bytes:  0 -  7
 * ret2 == Measurement value, bytes:  8 - 15
 * ret3 == Measurement value, bytes: 16 - 23
 * ret4 == Measurement value, bytes: 24 - 31
 * ret5 == Measurement value, bytes: 32 - 39
 * ret6 == Measurement value, bytes: 40 - 47
 * ret7 == Measurement value, bytes: 48 - 55
 * ret8 == Measurement value, bytes: 56 - 63
 */
#define SMC_TSI_MEASUREMENT_READ           SMC_TSI_FID(0x192)

/*
 * arg1  == Index, which measurements slot to extend
 * arg2  == Size of realm measurement in bytes, max 64 bytes
 * arg3  == Measurement value, bytes:  0 -  7
 * arg4  == Measurement value, bytes:  8 - 15
 * arg5  == Measurement value, bytes: 16 - 23
 * arg6  == Measurement value, bytes: 24 - 31
 * arg7  == Measurement value, bytes: 32 - 39
 * arg8  == Measurement value, bytes: 40 - 47
 * arg9  == Measurement value, bytes: 48 - 55
 * arg10 == Measurement value, bytes: 56 - 63
 * ret0  == Status / error
 */
#define SMC_TSI_MEASUREMENT_EXTEND         SMC_TSI_FID(0x193)

/*
 * arg1: Challenge value, bytes:  0 -  7
 * arg2: Challenge value, bytes:  8 - 15
 * arg3: Challenge value, bytes: 16 - 23
 * arg4: Challenge value, bytes: 24 - 31
 * arg5: Challenge value, bytes: 32 - 39
 * arg6: Challenge value, bytes: 40 - 47
 * arg7: Challenge value, bytes: 48 - 55
 * arg8: Challenge value, bytes: 56 - 63
 * ret0: Status / error
 * ret1: Upper bound on attestation token size in bytes
 */
#define SMC_TSI_ATTESTATION_TOKEN_INIT        SMC_TSI_FID(0x194)

/*
 * arg1: IPA of the Granule to which the token will be written
 * arg2: Offset within Granule to start of buffer in bytes
 * arg3: Size of buffer in bytes
 * ret0: Status / error
 * ret1: Number of bytes written to buffer
 */
#define SMC_TSI_ATTESTATION_TOKEN_CONTINUE  SMC_TSI_FID(0x195)

/*
 * arg1 == struct realm_config addr
 * ret0 == Status / error
 */
#define SMC_TSI_DEVICE_CERT                 SMC_TSI_FID(0x196)


#endif  /* __ASM_TSI_SMC_H_ */
