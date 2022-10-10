#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	struct bpf_link *link = NULL;
	struct bpf_program *prog;
	struct bpf_object *obj;
	char filename[256];
	int ret = 0;
	int flag1, flag2;
	char c;

	if (flag1 = (fcntl(STDIN_FILENO, F_GETFL, 0)) < 0) {
		perror("ERROR:fcntl error!\n");
		return -1;
	}
	flag2 = flag1 | O_NONBLOCK;
	fcntl(STDIN_FILENO, F_SETFL, flag2);

	snprintf(filename, sizeof(filename), "%s_kern.o", argv[0]);
	obj = bpf_object__open_file(filename, NULL);
	if (libbpf_get_error(obj)) {
		fprintf(stderr, "ERROR: opening BPF object file failed\n");
		return 0;
	}

	prog = bpf_object__find_program_by_name(obj, "bpf_prog1");

	if (!prog) {
		fprintf(stderr, "ERROR: finding a prog in obj file failed\n");
		goto cleanup;
	}

	/* load BPF program */
	if (bpf_object__load(obj)) {
		fprintf(stderr, "ERROR: loading BPF object file failed\n");
		goto cleanup;
	}

	link = bpf_program__attach(prog);
	if (libbpf_get_error(link)) {
		fprintf(stderr, "ERROR: bpf_program__attach failed\n");
		link = NULL;
		goto cleanup;
	}

	while (1) {
		if (scanf("%c", &c) >= 0) {
			goto cleanup;
		}
		else {
			fprintf(stderr, ".");
			sleep(1);
		}
	}

cleanup:
	bpf_link__destroy(link);
	bpf_object__close(obj);
	return ret ? 0 : 1;
}
