# Host unit tests (gcc + Unity). Run: make -C test
CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wshadow -O1 -g \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -I. -Iunity -I../common/inc -I../stm32_cluster/services -I../stm32_cluster/mcal
BUILD   := build

UNITY   := unity/unity.c
COMMON  := ../common/src/e2e.c ../common/src/can_matrix.c

TESTS   := $(BUILD)/test_com $(BUILD)/test_com_layer

.PHONY: all run clean vectors
all: run

$(BUILD):
	mkdir -p $@

# Signal packing and E2E, checked against DBC-generated vectors
$(BUILD)/test_com: test_com.c can_vectors.h $(COMMON) $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_com.c $(COMMON) $(UNITY)

# STM32 COM layer (timeouts, E2E rejection) running on the PC against a fake bxCAN driver
$(BUILD)/test_com_layer: test_com_layer.c fake_bxcan.c ../stm32_cluster/services/com.c $(COMMON) $(UNITY) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_com_layer.c fake_bxcan.c ../stm32_cluster/services/com.c $(COMMON) $(UNITY)

run: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done

# Regenerate DBC-derived vectors (needs: pip install cantools)
vectors:
	python3 ../tools/gen_test_vectors.py ../docs/can_matrix.dbc can_vectors.h

clean:
	rm -rf $(BUILD)
