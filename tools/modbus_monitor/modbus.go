package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"sync"
	"time"
)

// Modbus function codes used by the monitor.
const (
	fcReadDiscreteInputs   = 0x02
	fcReadHoldingRegisters = 0x03
	fcReadInputRegisters   = 0x04
	fcWriteSingleRegister  = 0x06
)

// ExceptionError is a Modbus exception response from the server.
type ExceptionError struct {
	Function byte
	Code     byte
}

func (e *ExceptionError) Error() string {
	return fmt.Sprintf("FC %02d exception 0x%02X", e.Function, e.Code)
}

// Client holds one persistent Modbus TCP connection; the board serves a
// single socket, so every request goes through it under a lock.
type Client struct {
	mu     sync.Mutex
	conn   net.Conn
	target string
	tid    uint16
}

func (c *Client) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.closeLocked()
}

func (c *Client) closeLocked() {
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
}

// call sends one PDU and returns the response PDU without its function code.
// A stale connection is reopened once before giving up.
func (c *Client) call(target string, pdu []byte) ([]byte, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	for attempt := 0; ; attempt++ {
		resp, err := c.roundTrip(target, pdu)
		var exc *ExceptionError
		if err == nil || errors.As(err, &exc) || attempt == 1 {
			return resp, err
		}
		c.closeLocked()
	}
}

func (c *Client) roundTrip(target string, pdu []byte) ([]byte, error) {
	if c.conn == nil || c.target != target {
		c.closeLocked()
		conn, err := net.DialTimeout("tcp", target, 500*time.Millisecond)
		if err != nil {
			return nil, err
		}
		c.conn, c.target = conn, target
	}
	c.conn.SetDeadline(time.Now().Add(500 * time.Millisecond))

	// MBAP header: transaction id, protocol id (0), length, unit id.
	c.tid++
	frame := make([]byte, 7, 7+len(pdu))
	binary.BigEndian.PutUint16(frame[0:], c.tid)
	binary.BigEndian.PutUint16(frame[4:], uint16(len(pdu)+1))
	frame[6] = 1
	if _, err := c.conn.Write(append(frame, pdu...)); err != nil {
		return nil, err
	}

	header := make([]byte, 7)
	if _, err := io.ReadFull(c.conn, header); err != nil {
		return nil, err
	}
	tid := binary.BigEndian.Uint16(header[0:])
	length := binary.BigEndian.Uint16(header[4:])
	if tid != c.tid || length < 2 {
		return nil, fmt.Errorf("bad MBAP header (tid %d, expected %d)", tid, c.tid)
	}
	body := make([]byte, length-1)
	if _, err := io.ReadFull(c.conn, body); err != nil {
		return nil, err
	}
	return checkResponse(pdu[0], body)
}

// checkResponse turns an exception PDU into an error and strips the function code.
func checkResponse(fc byte, body []byte) ([]byte, error) {
	switch {
	case body[0] == fc|0x80 && len(body) >= 2:
		return nil, &ExceptionError{Function: fc, Code: body[1]}
	case body[0] != fc:
		return nil, fmt.Errorf("function code mismatch: got %d, sent %d", body[0], fc)
	}
	return body[1:], nil
}

func request(fc byte, address, count uint16) []byte {
	return binary.BigEndian.AppendUint16(binary.BigEndian.AppendUint16([]byte{fc}, address), count)
}

func (c *Client) ReadBits(target string, fc byte, count uint16) ([]bool, error) {
	resp, err := c.call(target, request(fc, 0, count))
	if err != nil {
		return nil, err
	}
	return decodeBits(resp, count)
}

func (c *Client) ReadRegisters(target string, fc byte, count uint16) ([]uint16, error) {
	resp, err := c.call(target, request(fc, 0, count))
	if err != nil {
		return nil, err
	}
	return decodeRegisters(resp, count)
}

func (c *Client) WriteRegister(target string, address, value uint16) error {
	_, err := c.call(target, request(fcWriteSingleRegister, address, value))
	return err
}

// decodeBits unpacks a byte-count-prefixed bit field, LSB of the first byte first.
func decodeBits(resp []byte, count uint16) ([]bool, error) {
	if len(resp) < 1 || int(resp[0]) != (int(count)+7)/8 || len(resp) != 1+int(resp[0]) {
		return nil, errors.New("malformed bit response")
	}
	bits := make([]bool, count)
	for i := range bits {
		bits[i] = resp[1+i/8]>>(i%8)&1 == 1
	}
	return bits, nil
}

// decodeRegisters unpacks a byte-count-prefixed list of big-endian registers.
func decodeRegisters(resp []byte, count uint16) ([]uint16, error) {
	if len(resp) < 1 || int(resp[0]) != 2*int(count) || len(resp) != 1+int(resp[0]) {
		return nil, errors.New("malformed register response")
	}
	regs := make([]uint16, count)
	for i := range regs {
		regs[i] = binary.BigEndian.Uint16(resp[1+2*i:])
	}
	return regs, nil
}

// signed32 joins a high-word-first register pair into a signed 32-bit value.
func signed32(high, low uint16) int32 {
	return int32(uint32(high)<<16 | uint32(low))
}
