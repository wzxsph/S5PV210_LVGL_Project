import socket
import os
import struct
import threading
import sys

TFTP_PORT = 69
ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")

class TFTPServer:
    def __init__(self, host=None, port=TFTP_PORT, root=ROOT_DIR):
        self.port = port
        self.root = root
        self.running = False
        
        # Try to find a suitable bind address
        self.host = self._find_bind_host(host)
        
    def _find_bind_host(self, preferred):
        """Find a suitable host to bind to"""
        if preferred:
            # Verify preferred address exists
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.bind((preferred, 0))
                s.close()
                print(f"[TFTP] Using specified address: {preferred}")
                return preferred
            except OSError:
                print(f"[TFTP] Warning: Cannot bind to {preferred}, searching for alternative...")
        
        # List all interfaces and find 192.168.1.x
        # Method 1: Use socket to enumerate (works on all locales)
        try:
            import ipaddress
            hostname = socket.gethostname()
            for info in socket.getaddrinfo(hostname, None, socket.AF_INET):
                ip = info[4][0]
                if ip.startswith('192.168.1.'):
                    print(f"[TFTP] Found interface via getaddrinfo: {ip}")
                    return ip
        except Exception as e:
            print(f"[TFTP] getaddrinfo failed: {e}")

        # Method 2: Fallback to ipconfig parsing
        try:
            import subprocess, re
            result = subprocess.run(['ipconfig'], capture_output=True, text=True, timeout=5)
            # Match any line containing an IPv4 address in 192.168.1.x range
            for match in re.finditer(r'192\.168\.1\.\d{1,3}', result.stdout):
                ip = match.group(0)
                print(f"[TFTP] Found interface via ipconfig: {ip}")
                return ip
        except Exception as e:
            print(f"[TFTP] Could not run ipconfig: {e}")
        
        # Fallback to all interfaces
        print(f"[TFTP] Falling back to 0.0.0.0 (all interfaces)")
        return '0.0.0.0'
    
    def start(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.socket.bind((self.host, self.port))
        except OSError as e:
            print(f"[TFTP] Error binding to {self.host}:{self.port}: {e}")
            print("[TFTP] Trying 0.0.0.0...")
            self.host = '0.0.0.0'
            try:
                self.socket.bind((self.host, self.port))
            except OSError as e2:
                print(f"[TFTP] Fatal: Cannot bind to any address: {e2}")
                sys.exit(1)
        
        self.running = True
        print(f"[TFTP] Server started on {self.host}:{self.port}")
        print(f"[TFTP] Root directory: {self.root}")
        
        while self.running:
            try:
                self.socket.settimeout(1.0)
                data, client_addr = self.socket.recvfrom(516)
                threading.Thread(target=self.handle_request, args=(data, client_addr)).start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[TFTP] Error: {e}")
                    
    def stop(self):
        self.running = False
        self.socket.close()
        print("[TFTP] Server stopped")
        
    def handle_request(self, data, client_addr):
        opcode = struct.unpack('!H', data[:2])[0]
        
        if opcode == 1:
            filename = data[2:data.index(b'\x00', 2)].decode('ascii')
            print(f"[TFTP] RRQ from {client_addr[0]}:{client_addr[1]}: {filename}")
            self.send_file(filename, client_addr)
        elif opcode == 2:
            print(f"[TFTP] WRQ from {client_addr} (not supported)")
            
    def send_file(self, filename, client_addr):
        filepath = os.path.join(self.root, filename)
        
        if not os.path.exists(filepath):
            print(f"[TFTP] File not found: {filepath}")
            return
            
        filesize = os.path.getsize(filepath)
        total_blocks = (filesize + 511) // 512
        print(f"[TFTP] Sending: {filename} ({filesize} bytes, {total_blocks} blocks)")
        
        transfer_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        transfer_socket.settimeout(20.0)  # Generous timeout
        
        try:
            with open(filepath, 'rb') as f:
                block_num = 1
                
                while True:
                    block_data = f.read(512)
                    if not block_data:
                        break
                        
                    packet = struct.pack('!HH', 3, block_num) + block_data
                    
                    sent = False
                    for retry in range(20):  # More retries for unreliable networks
                        try:
                            transfer_socket.sendto(packet, client_addr)
                            ack, addr = transfer_socket.recvfrom(4)
                            
                            if addr != client_addr:
                                continue  # Ignore packets from wrong source
                                
                            ack_opcode, ack_block = struct.unpack('!HH', ack)
                            if ack_opcode == 4 and ack_block == block_num:
                                sent = True
                                break
                        except socket.timeout:
                            import time; time.sleep(0.3)
                    
                    if not sent:
                        print(f"[TFTP] FAILED: Timeout on block {block_num}/{total_blocks} after 20 retries")
                        return
                                
                    if block_num % 200 == 0 or block_num == 1:
                        pct = block_num * 100 // total_blocks
                        print(f"[TFTP] Progress: {block_num}/{total_blocks} blocks ({pct}%)")
                    
                    block_num += 1
                    
            print(f"[TFTP] Transfer complete: {filename} ({block_num-1} blocks, OK!)")
        except Exception as e:
            print(f"[TFTP] Error during transfer: {e}")
        finally:
            transfer_socket.close()

if __name__ == '__main__':
    server = TFTPServer()
    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()
