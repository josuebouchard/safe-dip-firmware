// Live Modbus TCP monitor for the board.
//
// Browsers can't open raw TCP sockets, so this serves index.html and bridges
// its HTTP polls to Modbus TCP.
//
//	cd tools/modbus_monitor && go run . [-http 127.0.0.1:8000]
package main

import (
	_ "embed"
	"encoding/json"
	"errors"
	"flag"
	"log"
	"net"
	"net/http"
	"strconv"
	"time"
)

//go:embed index.html
var indexHTML []byte

// Table layout, from the enums in App/app/modbus_map.cpp and App/app/faults.hpp
const (
	diCount = 7
	hrCount = 6

	irFaultsActive = 5
	irFaultFirst   = 6
	irFaultSources = 7
	faultCount     = 7
	irCount        = irFaultSources + faultCount
)

type State struct {
	EStop     bool     `json:"e_stop"`
	Start     bool     `json:"start"`
	Running   bool     `json:"running"`
	XAtTarget bool     `json:"x_at_target"`
	YAtTarget bool     `json:"y_at_target"`
	Fault     bool     `json:"fault"`
	Effector  bool     `json:"effector"`
	OpIndex   uint16   `json:"op_index"`
	X         int32    `json:"x"`
	Y         int32    `json:"y"`
	Params    []uint16 `json:"params"`
	LatencyMs float64  `json:"latency_ms"`

	FaultsActive uint16   `json:"faults_active"`
	FaultFirst   uint16   `json:"fault_first"`
	FaultSources []uint16 `json:"fault_sources"`
}

func poll(c *Client, target string) (*State, error) {
	start := time.Now()
	di, err := c.ReadBits(target, fcReadDiscreteInputs, diCount)
	if err != nil {
		return nil, err
	}
	// One request, so both position words and the fault registers each come
	// from the same moment.
	ir, err := c.ReadRegisters(target, fcReadInputRegisters, irCount)
	if err != nil {
		return nil, err
	}
	hr, err := c.ReadRegisters(target, fcReadHoldingRegisters, hrCount)
	if err != nil {
		return nil, err
	}
	state := newState(di, ir, hr)
	state.LatencyMs = float64(time.Since(start).Microseconds()) / 1000
	return state, nil
}

// newState maps the raw tables to their meaning.
func newState(di []bool, ir, hr []uint16) *State {
	return &State{
		EStop:     di[0],
		Start:     di[1],
		Running:   di[2],
		XAtTarget: di[3],
		YAtTarget: di[4],
		Effector:  di[5],
		Fault:     di[6],
		OpIndex:   ir[0],
		X:         signed32(ir[1], ir[2]),
		Y:         signed32(ir[3], ir[4]),
		Params:    hr,

		FaultsActive: ir[irFaultsActive],
		FaultFirst:   ir[irFaultFirst],
		FaultSources: ir[irFaultSources:],
	}
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}

func writeError(w http.ResponseWriter, status int, err error) {
	writeJSON(w, status, map[string]string{"error": err.Error()})
}

// target reads host and port from the query string as "host:port".
func target(r *http.Request) (string, error) {
	host := r.URL.Query().Get("host")
	port, err := strconv.ParseUint(r.URL.Query().Get("port"), 10, 16)
	if host == "" || err != nil || port == 0 {
		return "", errors.New("bad request: host and port (1-65535) required")
	}
	return net.JoinHostPort(host, strconv.FormatUint(port, 10)), nil
}

func main() {
	addr := flag.String("http", "127.0.0.1:8000", "HTTP listen address")
	flag.Parse()

	client := &Client{}
	mux := http.NewServeMux()

	mux.HandleFunc("GET /{$}", func(w http.ResponseWriter, _ *http.Request) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Write(indexHTML)
	})

	mux.HandleFunc("GET /api/poll", func(w http.ResponseWriter, r *http.Request) {
		t, err := target(r)
		if err != nil {
			writeError(w, http.StatusBadRequest, err)
			return
		}
		state, err := poll(client, t)
		if err != nil {
			writeError(w, http.StatusBadGateway, err)
			return
		}
		writeJSON(w, http.StatusOK, state)
	})

	mux.HandleFunc("POST /api/disconnect", func(w http.ResponseWriter, _ *http.Request) {
		client.Close()
		writeJSON(w, http.StatusOK, struct{}{})
	})

	mux.HandleFunc("POST /api/write", func(w http.ResponseWriter, r *http.Request) {
		t, err := target(r)
		address, aErr := strconv.ParseUint(r.URL.Query().Get("address"), 10, 16)
		value, vErr := strconv.ParseUint(r.URL.Query().Get("value"), 10, 16)
		if err != nil || aErr != nil || vErr != nil || address >= hrCount {
			writeError(w, http.StatusBadRequest, errors.New("bad request: host, port, address (0-5), value (0-65535) required"))
			return
		}
		if err := client.WriteRegister(t, uint16(address), uint16(value)); err != nil {
			writeError(w, http.StatusBadGateway, err)
			return
		}
		writeJSON(w, http.StatusOK, struct{}{})
	})

	log.Printf("Modbus monitor on http://%s", *addr)
	// Stops other sites in the browser from POSTing writes to the board.
	log.Fatal(http.ListenAndServe(*addr, http.NewCrossOriginProtection().Handler(mux)))
}
