import socket
import sys

SERVER_IP = '127.0.0.1'
SERVER_PORT = 12345

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((SERVER_IP, SERVER_PORT))
    print(f"Connected to {SERVER_IP}:{SERVER_PORT}")

    try:
        while True:
            cmd = input("Press Enter to send heartbeat, q to quit: ")
            if cmd.lower() == 'q':
                break

            heartbeat = bytes([0x01, 0x00, 0x00, 0x00])
            sock.sendall(heartbeat)
            print("Sent heartbeat")

            resp = sock.recv(1024)
            if not resp:
                print("Server disconnected")
                break

            print(f"Received {len(resp)} bytes: {' '.join(f'{b:02x}' for b in resp)}")

    finally:
        sock.close()

if __name__ == "__main__":
    main()
