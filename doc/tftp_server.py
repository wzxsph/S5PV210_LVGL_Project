import socket
import os
import struct
import threading
import sys
import argparse
import subprocess
import re

TFTP_PORT = 69
ROOT_DIR = os.path.dirname(os.path.abspath(__file__)) + "\\template-framebuffer-gui\\output"

# VPN/虚拟网卡关键词（用于检测和排除）
VIRTUAL_ADAPTER_KEYWORDS = [
    'vpn', 'virtual', 'tap-', 'tun-', 'openvpn', 'wireguard',
    'wan miniport', 'hyper-v', 'vmware', 'virtualbox',
    'kvm', 'qemu', 'veth', 'docker', 'bridge'
]

# 物理网卡关键词（优先选择）- 支持中英文
PHYSICAL_ADAPTER_KEYWORDS = [
    'ethernet', 'wi-fi', 'wireless', 'local area connection',
    '本地连接', '无线', 'realtek', 'intel', 'broadcom',
    '以太网', '以太', '物理'  # 中文支持
]


def get_all_interfaces():
    """获取所有网络接口信息"""
    interfaces = []
    
    try:
        result = subprocess.run(['ipconfig', '/all'], capture_output=True, text=True, timeout=10)
        lines = result.stdout.split('\n')
        
        current_adapter = None
        for line in lines:
            line = line.strip()
            
            # 检测适配器名称（必须包含"适配器"或"adapter"关键词）
            if line.endswith(':') and ('适配器' in line or 'adapter' in line.lower()):
                # 先保存之前的适配器（如果有）
                if current_adapter and (current_adapter['is_virtual'] or current_adapter['is_physical'] or current_adapter['ips']):
                    interfaces.append(current_adapter)
                
                adapter_name = line[:-1].strip()
                current_adapter = {
                    'name': adapter_name,
                    'description': adapter_name,
                    'ips': [],
                    'is_virtual': False,
                    'is_physical': False,
                    'priority': 999  # 数值越小优先级越高
                }
                
                # 检查是否为虚拟适配器
                adapter_lower = adapter_name.lower()
                for keyword in VIRTUAL_ADAPTER_KEYWORDS:
                    if keyword in adapter_lower:
                        current_adapter['is_virtual'] = True
                        break
                
                # 检查是否为物理适配器
                for keyword in PHYSICAL_ADAPTER_KEYWORDS:
                    if keyword in adapter_name:  # 支持中文匹配
                        current_adapter['is_physical'] = True
                        current_adapter['is_virtual'] = False  # 强制覆盖：物理网卡优先
                        current_adapter['priority'] -= 100  # 降低优先级数值（更优先）
                        break
            
            # 提取 IPv4 地址（支持中英文系统）
            elif current_adapter and ('IPv4' in line or 'ipv4' in line.lower() or 'IP Address' in line):
                match = re.search(r'(\d+\.\d+\.\d+\.\d+)', line)
                if match:
                    ip = match.group(1)
                    if not ip.startswith('127.') and not ip.startswith('169.254.'):
                        current_adapter['ips'].append(ip)
                        
    except Exception as e:
        print(f"[WARN] Could not get interface details: {e}")
    
    # 保存最后一个适配器
    if current_adapter and (current_adapter['is_virtual'] or current_adapter['is_physical'] or current_adapter['ips']):
        interfaces.append(current_adapter)
    
    return interfaces


def find_best_interface(preferred_ip=None, target_subnet='192.168.1'):
    """
    智能选择最佳网络接口
    
    策略：
    1. 如果用户指定了 IP，使用指定的 IP
    2. 优先选择物理网卡（非虚拟）
    3. 在目标子网中选择合适的 IP
    4. 排除已知的虚拟网卡
    """
    
    print("[NETWORK] Scanning network interfaces...")
    interfaces = get_all_interfaces()
    
    if not interfaces:
        print("[WARN] No interfaces found via ipconfig")
        return '0.0.0.0'
    
    # 显示所有找到的接口
    print("\n[NETWORK] Available interfaces:")
    print("-" * 80)
    for i, iface in enumerate(interfaces, 1):
        virtual_mark = "🔴 VIRTUAL" if iface['is_virtual'] else ("🟢 PHYSICAL" if iface['is_physical'] else "⚪ UNKNOWN")
        ips_str = ", ".join(iface['ips']) if iface['ips'] else "(no IPv4)"
        print(f"  {i:2d}. [{virtual_mark}] {iface['name'][:40]:<40} | IPs: {ips_str}")
    print("-" * 80)
    
    # 如果用户指定了首选 IP
    if preferred_ip:
        print(f"\n[NETWORK] User specified IP: {preferred_ip}")
        
        # 验证该 IP 是否存在且可用
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind((preferred_ip, 0))
            s.close()
            print(f"[OK] Using specified address: {preferred_ip}")
            return preferred_ip
        except OSError as e:
            print(f"[WARN] Cannot bind to {preferred_ip}: {e}")
            print("[NETWORK] Falling back to auto-detection...")
    
    # 自动选择最佳接口
    candidates = []
    
    for iface in interfaces:
        # 排除虚拟网卡
        if iface['is_virtual']:
            continue
            
        # 寻找目标子网的 IP
        for ip in iface['ips']:
            if ip.startswith(target_subnet):
                priority = iface['priority']
                # 额外偏好：如果 IP 以 .1 结尾（通常是网关/DHCP服务器），优先级更高
                if ip.endswith('.1'):
                    priority -= 5
                    
                candidates.append({
                    'ip': ip,
                    'adapter': iface['name'],
                    'priority': priority,
                    'is_physical': iface['is_physical']
                })
    
    # 如果在目标子网没找到，尝试所有非虚拟网卡的 IP
    if not candidates:
        print(f"[NETWORK] No interface in {target_subnet}.x subnet, checking all physical interfaces...")
        for iface in interfaces:
            if iface['is_virtual']:
                continue
            for ip in iface['ips']:
                candidates.append({
                    'ip': ip,
                    'adapter': iface['name'],
                    'priority': iface['priority'],
                    'is_physical': iface['is_physical']
                })
    
    # 如果还是没有，使用所有接口的 IP（包括虚拟网卡）
    if not candidates:
        print("[WARN] No physical interfaces found, including virtual adapters...")
        for iface in interfaces:
            for ip in iface['ips']:
                candidates.append({
                    'ip': ip,
                    'adapter': iface['name'],
                    'priority': iface['priority'] + 50,  # 降低虚拟网卡优先级
                    'is_physical': False
                })
    
    # 按优先级排序（数值越小越优先）
    candidates.sort(key=lambda x: x['priority'])
    
    if not candidates:
        print("[ERROR] No usable network interface found!")
        return '0.0.0.0'
    
    # 选择最佳候选
    best = candidates[0]
    
    print(f"\n[NETWORK] Auto-selected best interface:")
    print(f"       IP Address : {best['ip']}")
    print(f"       Adapter   : {best['adapter']}")
    print(f"       Type       : {'Physical ✅' if best['is_physical'] else 'Virtual ⚠️'}")
    print(f"       Priority   : {best['priority']}")
    
    # 尝试绑定到该 IP
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind((best['ip'], 0))
        s.close()
        print(f"[OK] Successfully validated: {best['ip']}")
        return best['ip']
    except OSError as e:
        print(f"[WARN] Cannot bind to {best['ip']}: {e}")
        
        # 尝试下一个候选
        for candidate in candidates[1:]:
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.bind((candidate['ip'], 0))
                s.close()
                print(f"[OK] Fallback to: {candidate['ip']} ({candidate['adapter']})")
                return candidate['ip']
            except OSError:
                continue
    
    # 最终回退
    print("[WARN] Cannot bind to any specific IP, using 0.0.0.0 (all interfaces)")
    return '0.0.0.0'


class TFTPServer:
    def __init__(self, host=None, port=TFTP_PORT, root=ROOT_DIR):
        self.port = port
        self.root = root
        self.running = False
        
        # 智能选择绑定地址
        self.host = find_best_interface(host)
        
    def start(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.socket.bind((self.host, self.port))
        except OSError as e:
            print(f"[ERROR] Error binding to {self.host}:{self.port}: {e}")
            print("[TFTP] Trying 0.0.0.0...")
            self.host = '0.0.0.0'
            try:
                self.socket.bind((self.host, self.port))
            except OSError as e2:
                print(f"[FATAL] Cannot bind to any address: {e2}")
                sys.exit(1)
        
        self.running = True
        print(f"\n[TFTP] ================================================")
        print(f"[TFTP] Server started successfully!")
        print(f"[TFTP] Listen Address : {self.host}:{self.port}")
        print(f"[TFTP] Root Directory  : {self.root}")
        print(f"[TFTP] Waiting for TFTP requests from Study210...")
        print(f"[TFTP] ================================================\n")
        
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
        print("\n[TFTP] Server stopped")
        
    def handle_request(self, data, client_addr):
        opcode = struct.unpack('!H', data[:2])[0]
        
        if opcode == 1:  # RRQ - Read Request
            filename = data[2:data.index(b'\x00', 2)].decode('ascii')
            print(f"[TFTP] 📥 RRQ from {client_addr[0]}:{client_addr[1]} -> '{filename}'")
            self.send_file(filename, client_addr)
        elif opcode == 2:  # WRQ - Write Request
            print(f"[TFTP] WRQ from {client_addr} (not supported)")
            
    def send_file(self, filename, client_addr):
        filepath = os.path.join(self.root, filename)
        
        if not os.path.exists(filepath):
            print(f"[TFTP] ❌ File not found: {filepath}")
            return
            
        filesize = os.path.getsize(filepath)
        total_blocks = (filesize + 511) // 512
        print(f"[TFTP] 📤 Sending: {filename} ({filesize:,} bytes, {total_blocks} blocks)")
        
        transfer_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        transfer_socket.settimeout(20.0)  # Generous timeout
        
        try:
            with open(filepath, 'rb') as f:
                block_num = 1
                start_time = __import__('time').time()
                
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
                        elapsed = __import__('time').time() - start_time
                        print(f"[TFTP] ❌ FAILED: Timeout on block {block_num}/{total_blocks} after 20 retries ({elapsed:.1f}s elapsed)")
                        return
                    
                    # Progress reporting
                    if block_num % 200 == 0 or block_num == 1 or block_num == total_blocks:
                        pct = block_num * 100 // total_blocks
                        elapsed = __import__('time').time() - start_time
                        speed = (block_num * 512 / 1024 / 1024) / elapsed if elapsed > 0 else 0
                        print(f"[TFTP] ⏳ Progress: {pct:3d}% ({block_num}/{total_blocks}) | Speed: {speed:.1f} MB/s")
                    
                    block_num += 1
                    
                total_time = __import__('time').time() - start_time
                avg_speed = (filesize / 1024 / 1024) / total_time if total_time > 0 else 0
                print(f"[TFTP] ✅ Transfer complete: {filename}")
                print(f"[TFTP]    Total time : {total_time:.2f}s")
                print(f"[TFTP]    Avg speed  : {avg_speed:.2f} MB/s")
                print(f"[TFTP]    Blocks     : {block_num-1}\n")
                
        except Exception as e:
            print(f"[TFTP] Error during transfer: {e}")
        finally:
            transfer_socket.close()


def main():
    parser = argparse.ArgumentParser(
        description='TFTP Server for S5PV210 LVGL Development',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tftp_server.py                          # Auto-detect best interface
  python tftp_server.py --host 192.168.1.100      # Use specific IP
  python tftp_server.py --subnet 192.168.0        # Target different subnet
  python tftp_server.py --port 69 --root ./output # Custom port and directory
        """
    )
    
    parser.add_argument('--host', '-H', type=str, default=None,
                       help='IP address to bind to (auto-detect if not specified)')
    parser.add_argument('--port', '-p', type=int, default=TFTP_PORT,
                       help=f'TFTP server port (default: {TFTP_PORT})')
    parser.add_argument('--root', '-r', type=str, default=ROOT_DIR,
                       help=f'Root directory for TFTP files (default: {ROOT_DIR})')
    parser.add_argument('--subnet', '-s', type=str, default='192.168.1',
                       help='Target subnet prefix (default: 192.168.1)')
    parser.add_argument('--list-interfaces', '-l', action='store_true',
                       help='List all network interfaces and exit')
    
    args = parser.parse_args()
    
    # 仅列出接口模式
    if args.list_interfaces:
        print("\n=== Network Interface List ===\n")
        interfaces = get_all_interfaces()
        for i, iface in enumerate(interfaces, 1):
            virtual_mark = "[V]" if iface['is_physical'] else ("[P]" if iface['is_physical'] else "[?]")
            ips_str = ", ".join(iface['ips']) if iface['ips'] else "(none)"
            print(f"{i:2d}. {virtual_mark} {iface['name']:<45} | {ips_str}")
        print("\nLegend: [P]=Physical [V]=Virtual [?]=Unknown")
        return
    
    # 启动服务器
    server = TFTPServer(host=args.host, port=args.port, root=args.root)
    try:
        server.start()
    except KeyboardInterrupt:
        print("\n\n[TFTP] Received interrupt signal...")
        server.stop()
        print("[TFTP] Goodbye!")


if __name__ == '__main__':
    main()
