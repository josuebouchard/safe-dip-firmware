package main

import (
	"errors"
	"slices"
	"testing"
)

func TestDecode(t *testing.T) {
	bits, err := decodeBits([]byte{1, 0b10101}, 5)
	if err != nil || !slices.Equal(bits, []bool{true, false, true, false, true}) {
		t.Fatalf("decodeBits = %v, %v", bits, err)
	}

	regs, err := decodeRegisters([]byte{4, 0x00, 0x07, 0xFF, 0xFF}, 2)
	if err != nil || !slices.Equal(regs, []uint16{7, 0xFFFF}) {
		t.Fatalf("decodeRegisters = %v, %v", regs, err)
	}

	if _, err := decodeRegisters([]byte{2, 0x00, 0x07}, 2); err == nil {
		t.Fatal("short register response accepted")
	}

	if signed32(0xFFFF, 0xFFFF) != -1 || signed32(0x0001, 0x0000) != 65536 {
		t.Fatal("signed32 wrong")
	}

	var exc *ExceptionError
	if _, err := checkResponse(6, []byte{0x86, 0x04}); !errors.As(err, &exc) || exc.Code != 4 {
		t.Fatalf("checkResponse = %v", err)
	}
}
