package main

import (
	"errors"
	"slices"
	"testing"
)

func TestNewStateMapsFaultRegisters(t *testing.T) {
	di := make([]bool, diCount)
	di[7] = true // Fault active
	ir := make([]uint16, irCount)
	ir[0] = 6
	ir[irFaultsActive] = 3
	ir[irFaultFirst] = 2

	s := newState(di, ir, make([]uint16, hrCount))

	if !s.Fault || s.Effector {
		t.Fatalf("Fault = %v, Effector = %v", s.Fault, s.Effector)
	}
	if s.FaultsActive != 3 || s.FaultFirst != 2 {
		t.Fatalf("FaultsActive = %d, FaultFirst = %d", s.FaultsActive, s.FaultFirst)
	}
}

func TestNewStateMapsLoopTime(t *testing.T) {
	ir := make([]uint16, irCount)
	ir[irLoopTime] = 42
	ir[irLoopTimeMax] = 900

	s := newState(make([]bool, diCount), ir, make([]uint16, hrCount))

	if s.LoopTimeUs != 42 || s.LoopTimeMaxUs != 900 {
		t.Fatalf("LoopTimeUs = %d, LoopTimeMaxUs = %d", s.LoopTimeUs, s.LoopTimeMaxUs)
	}
}

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
