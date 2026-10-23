# Host unit tests (gcc + Unity). Run: make -C test
CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -O1 -g \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -I. -Iunity -I../common/inc -I../stm32_cluster/services -I../stm32_cluster/mcal
BUILD   := build

UNITY   := unity/unity.c
COMMON  := ../common/src/e2e.c ../common/src/can_matrix.c ../common/src/isotp.c
DIAG    := ../stm32_cluster/services/nvm.c ../stm32_cluster/services/dem.c \
           ../stm32_cluster/services/uds.c fake_flash.c

TESTS   := $(BUILD)/test_com $(BUILD)/test_isotp $(BUILD)/test_diag

.PHONY: all run clean vectors
all: run

$(BUILD):
	mkdir -p $@

$(BUILD)/test_com: test_com.c can_vectors.h $(COMMON) $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_com.c $(COMMON) $(UNITY)

$(BUILD)/test_isotp: test_isotp.c $(COMMON) $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_isotp.c ../common/src/isotp.c $(UNITY)

$(BUILD)/test_diag: test_diag.c $(DIAG) $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_diag.c $(DIAG) $(UNITY)

run: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done

# Regenerate DBC-derived vectors (needs: pip install cantools)
vectors:
	python3 ../tools/gen_test_vectors.py ../docs/can_matrix.dbc can_vectors.h

clean:
	rm -rf $(BUILD)
