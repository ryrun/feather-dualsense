package main

import (
	"encoding/binary"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"sync"
	"time"
)

const (
	reportID      = 0x7e
	payloadLength = 63
	packetLength  = 64

	statusUsagePage = 0xff42
	statusUsage     = 0x01
)

type touchPoint struct {
	Active bool   `json:"active"`
	ID     uint8  `json:"id"`
	X      uint16 `json:"x"`
	Y      uint16 `json:"y"`
}

type vector3 struct {
	X int16 `json:"x"`
	Y int16 `json:"y"`
	Z int16 `json:"z"`
}

type statusReport struct {
	ReportID       uint8      `json:"reportId"`
	Version        uint8      `json:"version"`
	ControllerType uint8      `json:"controllerType"`
	Sequence       uint16     `json:"sequence"`
	Buttons        string     `json:"buttons"`
	LeftX          uint8      `json:"leftX"`
	LeftY          uint8      `json:"leftY"`
	RightX         uint8      `json:"rightX"`
	RightY         uint8      `json:"rightY"`
	L2             uint8      `json:"l2"`
	R2             uint8      `json:"r2"`
	Flags          uint16     `json:"flags"`
	Touch0         touchPoint `json:"touch0"`
	Touch1         touchPoint `json:"touch1"`
	Gyro           vector3    `json:"gyro"`
	Accel          vector3    `json:"accel"`
	LeanRoll       int16      `json:"leanRollCentideg"`
}

type hub struct {
	mu      sync.Mutex
	clients map[chan []byte]struct{}
	latest  []byte
}

func newHub() *hub {
	return &hub{clients: make(map[chan []byte]struct{})}
}

func (h *hub) add() chan []byte {
	ch := make(chan []byte, 8)
	h.mu.Lock()
	h.clients[ch] = struct{}{}
	if h.latest != nil {
		ch <- h.latest
	}
	h.mu.Unlock()
	return ch
}

func (h *hub) remove(ch chan []byte) {
	h.mu.Lock()
	delete(h.clients, ch)
	close(ch)
	h.mu.Unlock()
}

func (h *hub) publish(message []byte) {
	h.mu.Lock()
	h.latest = message
	for ch := range h.clients {
		select {
		case ch <- message:
		default:
			<-ch
			ch <- message
		}
	}
	h.mu.Unlock()
}

func main() {
	addr := flag.String("addr", "127.0.0.1:8765", "HTTP listen address")
	rootFlag := flag.String("root", "", "repository root to serve")
	flag.Parse()

	root, err := resolveRoot(*rootFlag)
	if err != nil {
		log.Fatal(err)
	}

	h := newHub()
	go readStatusReports(h)

	mux := http.NewServeMux()
	mux.HandleFunc("/events", h.events)
	mux.HandleFunc("/", overlayHandler(root))

	log.Printf("Serving overlay files from %s", root)
	log.Printf("OBS URL: http://%s/tools/status_overlay.html?input=sse", *addr)
	log.Fatal(http.ListenAndServe(*addr, mux))
}

func overlayHandler(root string) http.HandlerFunc {
	overlayPath := filepath.Join(root, "tools", "status_overlay.html")
	modelPath := filepath.Join(root, "tools", "dualsenseende.obj")

	return func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/", "/status_overlay.html", "/tools/status_overlay.html":
			http.ServeFile(w, r, overlayPath)
		case "/dualsenseende.obj", "/tools/dualsenseende.obj":
			http.ServeFile(w, r, modelPath)
		default:
			http.NotFound(w, r)
		}
	}
}

func (h *hub) events(w http.ResponseWriter, r *http.Request) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	fmt.Fprint(w, ": connected\n\n")
	flusher.Flush()

	ch := h.add()
	defer h.remove(ch)

	ticker := time.NewTicker(15 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case message := <-ch:
			fmt.Fprintf(w, "data: %s\n\n", message)
			flusher.Flush()
		case <-ticker.C:
			fmt.Fprint(w, ": keepalive\n\n")
			flusher.Flush()
		case <-r.Context().Done():
			return
		}
	}
}

func readStatusReports(h *hub) {
	packets := make(chan []byte, 32)
	go func() {
		if err := readStatusPackets(packets); err != nil {
			log.Printf("Status HID reader stopped: %v", err)
		}
	}()

	for {
		packet := <-packets
		payload, ok := statusPayload(packet)
		if !ok {
			continue
		}
		report := parsePayload(payload)
		message, err := json.Marshal(report)
		if err != nil {
			log.Printf("JSON encode failed: %v", err)
			continue
		}
		h.publish(message)
	}
}

func statusPayload(packet []byte) ([]byte, bool) {
	if len(packet) == payloadLength {
		return packet, true
	}
	if len(packet) >= packetLength && packet[0] == reportID {
		return packet[1:packetLength], true
	}
	return nil, false
}

func parsePayload(payload []byte) statusReport {
	flags := binary.LittleEndian.Uint16(payload[18:20])
	return statusReport{
		ReportID:       reportID,
		Version:        payload[0],
		ControllerType: payload[1],
		Sequence:       binary.LittleEndian.Uint16(payload[2:4]),
		Buttons:        fmt.Sprintf("%016x", binary.LittleEndian.Uint64(payload[4:12])),
		LeftX:          payload[12],
		LeftY:          payload[13],
		RightX:         payload[14],
		RightY:         payload[15],
		L2:             payload[16],
		R2:             payload[17],
		Flags:          flags,
		Touch0: touchPoint{
			Active: flags&(1<<1) != 0,
			ID:     payload[20],
			X:      binary.LittleEndian.Uint16(payload[22:24]),
			Y:      binary.LittleEndian.Uint16(payload[24:26]),
		},
		Touch1: touchPoint{
			Active: flags&(1<<2) != 0,
			ID:     payload[21],
			X:      binary.LittleEndian.Uint16(payload[26:28]),
			Y:      binary.LittleEndian.Uint16(payload[28:30]),
		},
		Gyro: vector3{
			X: int16(binary.LittleEndian.Uint16(payload[30:32])),
			Y: int16(binary.LittleEndian.Uint16(payload[32:34])),
			Z: int16(binary.LittleEndian.Uint16(payload[34:36])),
		},
		Accel: vector3{
			X: int16(binary.LittleEndian.Uint16(payload[36:38])),
			Y: int16(binary.LittleEndian.Uint16(payload[38:40])),
			Z: int16(binary.LittleEndian.Uint16(payload[40:42])),
		},
		LeanRoll: int16(binary.LittleEndian.Uint16(payload[42:44])),
	}
}

func resolveRoot(explicit string) (string, error) {
	if explicit != "" {
		return requireOverlay(explicit)
	}

	var candidates []string
	if cwd, err := os.Getwd(); err == nil {
		candidates = append(candidates, cwd, filepath.Join(cwd, ".."), filepath.Join(cwd, "..", ".."))
	}
	if exe, err := os.Executable(); err == nil {
		dir := filepath.Dir(exe)
		candidates = append(candidates, dir, filepath.Join(dir, ".."), filepath.Join(dir, "..", ".."))
	}
	if _, file, _, ok := runtime.Caller(0); ok {
		dir := filepath.Dir(file)
		candidates = append(candidates, dir, filepath.Join(dir, ".."), filepath.Join(dir, "..", ".."))
	}

	for _, candidate := range candidates {
		if root, err := requireOverlay(candidate); err == nil {
			return root, nil
		}
	}
	return "", fmt.Errorf("could not locate repository root; pass -root /path/to/feather-dualsense")
}

func requireOverlay(path string) (string, error) {
	root, err := filepath.Abs(path)
	if err != nil {
		return "", err
	}
	overlay := filepath.Join(root, "tools", "status_overlay.html")
	if _, err := os.Stat(overlay); err != nil {
		return "", err
	}
	return root, nil
}
