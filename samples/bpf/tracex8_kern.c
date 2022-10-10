#include <uapi/linux/ptrace.h>
#include <uapi/linux/bpf.h>
#include <linux/version.h>
#include <bpf/bpf_helpers.h>

SEC("kprobe/test_input_2")
int bpf_prog1(struct pt_regs *ctx)
{
	unsigned long rc = 7;
	bpf_override_regs(ctx, 14, rc);
	return 0;
}

char _license[] SEC("license") = "GPL";
u32 _version SEC("version") = LINUX_VERSION_CODE;
